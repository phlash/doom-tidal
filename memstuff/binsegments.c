#include <stdio.h>
#include <stdint.h>
#include <string.h>

#pragma pack(push,1)
typedef struct {
	uint8_t chip;
	uint8_t segs;
	uint8_t ftype;
	uint8_t fszsp;
	uint32_t entry;
	uint8_t _pad[16];
} esp_binfile_hdr_t;

typedef struct {
	uint32_t load;
	uint32_t size;
} esp_segment_hdr_t;
#pragma pack(pop)

int main(int argc, char **argv) {
	FILE *binf = stdin;
	int off = 0;
	for (int a=1; a<argc; a++) {
		if (strncmp(argv[a], "-o", 2)==0)
			sscanf(argv[++a], "%i", &off);
		else
			binf = fopen(argv[a], "rb");
	}
	// the ESP binfile header
	esp_binfile_hdr_t hdr;
	if (fread(&hdr, sizeof(hdr), 1, binf)!=1) {
		perror("reading header");
		return 1;
	}
	printf("offset: 0x%x\n" \
		"chip: %d\n" \
		"segs: %d\n" \
		"ftype:%d\n" \
		"fszsp:0x%x\n" \
		"entry: 0x%x\n",
		off, hdr.chip, hdr.segs, hdr.ftype, hdr.fszsp, hdr.entry);
	// the segments..
	for (uint8_t seg=0; seg<hdr.segs; seg++) {
		esp_segment_hdr_t s;
		if (fread(&s, sizeof(s), 1, binf)!=1) {
			perror("reading segment");
			return 1;
		}
		printf("%d: off=%x load=%x size=%x\n", seg, off+ftell(binf), s.load, s.size);
		if (fseek(binf, s.size, SEEK_CUR)<0) {
			perror("seeking binfile");
			return 1;
		}
	}
	return 0;
}
