/* Include the production loader to exercise the private on-disk name cache. */
#include "../src/main.cpp"

static void clear_names()
{
  for (size_t i = 0; i < numberOfCards; ++i) free(idToName[i]);
  free(idToName);
  idToName = nullptr;
  numberOfCards = 0;
}

static bool write_fixture(const char* path, uint32_t name_offset, bool truncated)
{
  FILE* file = fopen(path, "wb");
  if (!file) return false;
  const uint32_t count = 2;
  const char text[] = "A/B\0Test Name";
  const uint32_t text_size = sizeof(text);
  card_disk_record_t records[2] = {};
  records[0].id = 1;
  records[0].full_name_offset = name_offset;
  records[1].id = 0;
  records[1].full_name_offset = 4;
  fwrite(&count, sizeof(count), 1, file);
  fwrite(&text_size, sizeof(text_size), 1, file);
  if (!truncated) {
    fwrite(records, sizeof(records), 1, file);
    fwrite(text, sizeof(text), 1, file);
  }
  return fclose(file) == 0;
}

int main(int argc, char** argv)
{
  if (argc != 2) return 1;
  const char* path = argv[1];
  if (!write_fixture(path, 0, false) || !loadDat(path)) return 2;
  if (numberOfCards != 2 || strcmp(idToName[0], "Test Name") || strcmp(idToName[1], "A_B"))
    return 3;
  clear_names();
  if (!write_fixture(path, UINT32_MAX, false) || loadDat(path) || idToName) return 4;
  if (!write_fixture(path, 0, true) || loadDat(path) || idToName) return 5;
  remove(path);
  return 0;
}
