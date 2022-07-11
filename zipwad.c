// Run WAD assets through gzip, if they are smaller, re-write with custom header ('ZL<size>')
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

// ignore lumps smaller than this..
#define MINZIPLUMP		2048

// write blocks up to this size through gzip
#define MAXPIPEWR		4096

#pragma pack(push,1)
typedef struct {
	char ident[4];
	uint32_t count;
	uint32_t offset;
} wad_header_t;

typedef struct {
	uint32_t offset;
	uint32_t size;
	char name[8];
} lump_t;

typedef struct {
	char sigz, sigl;
	uint32_t clen;
} lumpz_t;
#pragma pack(pop)

int usage(char **argv) {
	return printf("usage: %s [-v] -w <wad in> -o <wad out>\n", argv[0]);
}

int main(int argc, char **argv) {
	FILE *wadin, *wadout;
	wad_header_t hdr;
	lump_t *lumps;
	int verb = 0;
	int rv = 1;
	for (int arg=1; arg<argc; arg++) {
		if (strncmp(argv[arg], "-w", 2)==0)
			wadin = fopen(argv[++arg], "rb");
		else if (strncmp(argv[arg], "-o", 2)==0)
			wadout = fopen(argv[++arg], "w+b");
		else if (strncmp(argv[arg], "-v", 2)==0)
			verb++;
		else
			return usage(argv);
	}
	if (!wadin || !wadout)
		return usage(argv);
	if (fread(&hdr, sizeof(hdr), 1, wadin)!=1) {
		perror("reading wad in header");
		goto oops;
	}
	if (fwrite(&hdr, sizeof(hdr), 1, wadout)!=1) {
		perror("writing wad out header");
		goto oops;
	}
	if (verb) printf("WAD in: ident=%.4s count=%d index=%x\n",
		hdr.ident, hdr.count, hdr.offset);
	lumps = calloc(sizeof(lump_t), hdr.count);
	if (!lumps) {
		perror("allocating index table");
		goto oops;
	}
	if (fseek(wadin, hdr.offset, SEEK_SET)<0) {
		perror("seeking wad in for index");
		goto oops;
	}
	if (fread(lumps, sizeof(lump_t), hdr.count, wadin)!=hdr.count) {
		perror("reading wad in index");
		goto oops;
	}
	// work through lumps, try gzipping bigger ones..
	if (verb) puts("index: name     size/hex offset/hex");
	for (uint32_t lump=0; lump<hdr.count; lump++) {
		if (verb) {
			printf("%5d: %-8.8s %08x %08x",
			lump,
			lumps[lump].name,
			lumps[lump].size,
			lumps[lump].offset);
			fflush(stdout);
		}
		// fixup offset for current lump (as we may have shrunk earlier stuff)
		uint32_t inoff = lumps[lump].offset;
		lumps[lump].offset = ftell(wadout);
		// nothing to read?
		if (!lumps[lump].size) {
			puts(" [empty]");
			continue;
		}
		if (fseek(wadin, inoff, SEEK_SET)<0) {
			perror("seeking wad in for lump");
			goto oops;
		}
		char *buf = malloc(lumps[lump].size);
		if (!buf) {
			perror("allocating lump buffer");
			goto oops;
		}
		if (fread(buf, lumps[lump].size, 1, wadin)!=1) {
			perror("reading wad in for lump");
			goto oops;
		}
		// skip small lumps
		if (lumps[lump].size < MINZIPLUMP) {
			if (fwrite(buf, lumps[lump].size, 1, wadout)!=1) {
				perror("writing wad out same lump");
				goto oops;
			}
			free(buf);
			if (verb) puts(" [skipped]");
			continue;
		}
		char *rbuf = malloc(lumps[lump].size);
		if (!rbuf) {
			perror("allocating zip return buffer");
			goto oops;
		}
		#define PIPERD	0
		#define PIPEWR	1
		int pipeout[2];
		int pipein[2];
		if (pipe(pipeout)<0 || pipe(pipein)<0) {
			perror("creating pipes for gzip");
			goto oops;
		}
		if (!fork()) {
			// child - connect gzip and go
			close(pipeout[PIPEWR]);
			close(pipein[PIPERD]);
			dup2(pipeout[PIPERD], 0);
			dup2(pipein[PIPEWR], 1);
			execlp("gzip", "gzip", "-f", NULL);
			perror("exec gzip");
			goto oops;
		}
		// parent, push asset through gzip
		close(pipeout[PIPERD]);
		close(pipein[PIPEWR]);
		fcntl(pipein[PIPERD], F_SETFL, O_NONBLOCK);
		uint32_t gzw = 0;
		uint32_t gzr = 0;
		while (1) {
			if (gzw < lumps[lump].size) {
				// write another chunk..
				uint32_t n = lumps[lump].size - gzw;
				if (n > MAXPIPEWR)
					n = MAXPIPEWR;
				if (verb>1) { printf(" w%d", n); fflush(stdout); }
				if (write(pipeout[PIPEWR], buf+gzw, n)!=n) {
					perror("writing gzip");
					goto oops;
				}
				gzw += n;
			} else if (pipeout[PIPEWR]) {
				// done, close output pipe, indicates EOF
				if (verb>1) { printf(" co"); fflush(stdout); }
				close(pipeout[PIPEWR]);
				pipeout[PIPEWR] = 0;
			}
			if (gzr < lumps[lump].size) {
				int n = lumps[lump].size - gzr;
				n = read(pipein[PIPERD], rbuf+gzr, n);
				if (n<0 && EWOULDBLOCK==errno)
					continue;
				if (verb>1) { printf(" r%d", n); fflush(stdout); }
				if (n<0) {
					perror("reading gzip output");
					goto oops;
				} else if (0==n) {
					// EOF from gzip, we're done
					break;
				}
				gzr += n;
			} else if (pipein[PIPERD]) {
				// too big, close input pipe, nukes gzip (EPIPE)
				if (verb>1) { printf(" ci"); fflush(stdout); }
				close(pipein[PIPERD]);
				pipein[PIPERD] = 0;
			}
		}
		// tidy up
		if (pipeout[PIPEWR]) close(pipeout[PIPEWR]);
		if (pipein[PIPERD]) close(pipein[PIPERD]);
		int sz;
		wait(&sz);
		if (!WIFEXITED(sz) || WEXITSTATUS(sz)!=0) {
			perror("gzip terminated badly");
			goto oops;
		}
		if (verb) printf(" [%08x]\n", gzr);
		// if no smaller, write input lump
		if (gzr >= gzw) {
			if (fwrite(buf, lumps[lump].size, 1, wadout)!=1) {
				perror("writing wad out same lump");
				goto oops;
			}
		} else {
			// adjust to remove gzip file header
			memmove(rbuf, rbuf+10, gzr-10);
			gzr -= 10;
			// honey I shrunk the kid.. write custom header + gzipped lump
			lumpz_t zh = { 'Z', 'L', gzr };
			if (fwrite(&zh, sizeof(zh), 1, wadout)!=1 ||
				fwrite(rbuf, gzr, 1, wadout)!=1) {
				perror("writing wad out zip lump");
				goto oops;
			}
		}
		free(rbuf);
		free(buf);
	}
	// update index table offset
	hdr.offset = ftell(wadout);
	// write index table
	if (fwrite(lumps, sizeof(lump_t), hdr.count, wadout)!=hdr.count) {
		perror("writing wad out index");
		goto oops;
	}
	// rewrite header
	if (fseek(wadout, 0, SEEK_SET)<0 ||
		fwrite(&hdr, sizeof(hdr), 1, wadout)!=1) {
		perror("rewriting wad out header");
		goto oops;
	}
	// et voila!
	rv = 0;
oops:
	if (wadin)
		fclose(wadin);
	if (wadout)
		fclose(wadout);
	return rv;
}
