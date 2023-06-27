#include <assert.h>
#include <mach-o/loader.h>
#include <stdlib.h>
#include <stdio.h>

// $ ./atom-info <mach-o mergeable library path>
// This tool shows link edit info stored in LC_ATOM_INFO payload

void dump_bytes(void *buffer, size_t size, unsigned int offset) {

  // Simple hexdump -C implementation

  unsigned char *p = buffer;
  unsigned char *end = p + size;
  size_t chunk_size = 16;
  while (p < end) {
    if ((p - (unsigned char *)buffer) % chunk_size == 0) {
      printf("%08lx ", offset + ((p - (unsigned char *)buffer)));
    }
    printf("%02x ", *p);
    p++;
    if ((p - (unsigned char *)buffer) % chunk_size == 0) {
      // Print ASCII interpretation if possible

      for (size_t i = 0; i < chunk_size; i++) {
        if (p - chunk_size + i < end) {
          unsigned char c = *(p - chunk_size + i);
          if (c >= 32 && c < 127) {
            printf("%c", c);
          } else {
            printf(".");
          }
        }
      }
      printf("\n");
    }
  }
  printf("\n");
}

void parse_lc_atom_info(FILE *fp) {
  size_t lc_start = ftell(fp);
  struct linkedit_data_command lc;
  fread(&lc, sizeof(typeof(lc)), 1, fp);

  printf("LC_ATOM_INFO\n");
  printf("  cmdsize %u\n", lc.cmdsize);
  printf("  offset %u\n", lc.dataoff);
  printf("  size %u\n", lc.datasize);

  void *buffer;

  buffer = malloc(lc.datasize);

  fseek(fp, lc.dataoff, SEEK_SET);
  fread(buffer, lc.datasize, 1, fp);

  dump_bytes(buffer, lc.datasize, lc.dataoff);

  free(buffer);
}

void parse_load_commands(FILE *fp, struct mach_header_64 *mh) {
  for (uint32_t i = 0; i < mh->ncmds; i++) {
    struct load_command lc;
    size_t lc_start = ftell(fp);

    fread(&lc, sizeof(struct load_command), 1, fp);

    if (lc.cmd == LC_ATOM_INFO) {
      fseek(fp, lc_start, SEEK_SET);
      parse_lc_atom_info(fp);
    }
    fseek(fp, lc_start, SEEK_SET);
    fseek(fp, lc.cmdsize, SEEK_CUR);
  }
}

int main(int argc, char **argv) {

  if (argc < 2) {
    printf("Error: No file specified\n");
    return 1;
  }

  char *input_file = argv[1];
  FILE *file = fopen(input_file, "r");
  if (file == NULL) {
    printf("Error: Could not open file %s\n", input_file);
    return 1;
  }

  struct mach_header_64 header;
  fread(&header, sizeof(header), 1, file);

  if (header.magic != MH_MAGIC_64) {
    printf("Error: %s is not a valid Mach-O 64-bit file\n", input_file);
    return 1;
  }

  printf("Mach-O 64-bit file\n");

  parse_load_commands(file, &header);

  fclose(file);
  return 0;
}
