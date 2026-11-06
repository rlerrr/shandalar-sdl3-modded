#include "defs.h"
#include "CardArtLib.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "platform/file_io.h"
#include "platform/cards_dat.h"
#include <vector>

#ifndef _MSC_VER
#include <unistd.h>
#endif


static std::unordered_map<std::string, SDL_Surface*> *cache;
static char** idToName;
static size_t numberOfCards = 0;
static CRITICAL_SECTION crit;
static bool images_counted = false;

static card_ptr_t* cards_ptr_shandalar_dll = NULL;

static bool loadDat(const char* fname)
{
  SDL_IOStream* f = shandalar_open_file_io(fname, "rb");
  if (!f)
    return false;
  uint32_t cardCount = 0, stringDataSize = 0;
  bool header_ok = shandalar_io_read(&cardCount, sizeof(cardCount), 1, f) == 1 &&
                   shandalar_io_read(&stringDataSize, sizeof(stringDataSize), 1, f) == 1;
  Sint64 fileSize = SDL_GetIOSize(f);
  if (!header_ok || !cardCount || !stringDataSize || fileSize < 8 ||
      uint64_t(cardCount) * sizeof(card_disk_record_t) + stringDataSize > uint64_t(fileSize - 8)) {
    shandalar_io_close(f);
    return SDL_SetError("Invalid Cards.dat name-table header");
  }
  std::vector<card_disk_record_t> cardData(cardCount);
  std::vector<char> stringData(stringDataSize);
  bool data_ok = shandalar_io_read(cardData.data(), sizeof(cardData[0]), cardCount, f) == cardCount &&
                 shandalar_io_read(stringData.data(), 1, stringDataSize, f) == stringDataSize;
  shandalar_io_close(f);
  if (!data_ok)
    return SDL_SetError("Incomplete Cards.dat name table");

  bool okchar[256] = {0};
  for (int i = 'a'; i <= 'z'; ++i) okchar[i] = true;
  for (int i = 'A'; i <= 'Z'; ++i) okchar[i] = true;
  for (int i = '0'; i <= '9'; ++i) okchar[i] = true;
  for (unsigned char ch : {',', ' ', '\'', '-'}) okchar[ch] = true;

  for (const card_disk_record_t& entry : cardData) {
    if (entry.id < 0 || uint32_t(entry.id) >= cardCount || entry.full_name_offset >= stringDataSize ||
        !memchr(stringData.data() + entry.full_name_offset, 0, stringDataSize - entry.full_name_offset))
      return SDL_SetError("Invalid Cards.dat card-name offset or ID");
  }
  char** names = (char**)calloc(cardCount, sizeof(*names));
  if (!names)
    return SDL_SetError("Could not allocate card-art name table");
  for (const card_disk_record_t& entry : cardData) {
    const char* name = stringData.data() + entry.full_name_offset;
    char* copy = (char*)malloc(strlen(name) + 1);
    if (!copy) {
      for (uint32_t i = 0; i < cardCount; ++i) free(names[i]);
      free(names);
      return SDL_SetError("Could not allocate card-art name");
    }
    strcpy(copy, name);
    for (char* p = copy; *p; ++p)
      if (!okchar[(unsigned char)*p]) *p = '_';
    free(names[entry.id]);
    names[entry.id] = copy;
  }
  numberOfCards = cardCount;
  idToName = names;
  return true;
}

static void load_names(void)
{
	// Precondition: idToName == NULL

#ifdef SHANDALAR_SDL_WIN32_COMPAT
	loadDat("Cards.dat");
#else
	HMODULE parent = GetModuleHandle(0);

	if (GetProcAddress(parent, "szDeckName")) { //manalink
		char* cards_dat = (char*)0x4D722E;
		loadDat(cards_dat);
	} else if (GetModuleHandle("deck.exe")) { //deck.exe
		loadDat("Cards.dat");
	} else { // shandalar
		loadDat("Cards.dat");
	}
#endif
}

static std::string idToNameFun(int id, int version) {
	if (idToName == NULL){
		load_names();
	}
	if (!idToName || id < 0 || (size_t)id >= numberOfCards || !idToName[id])
		return std::string();
	char tmp[254];
	if (version > 0) {
		sprintf(tmp, "CardArtManalink/%s (%d).jpg", idToName[id], version);
	} else {
		sprintf(tmp, "CardArtManalink/%s.jpg", idToName[id]);
	}

	std::string path = tmp;
	return path;
}

static void count_images(void)
{
	// Precondition: images_counted == false
	// Precondition: within critical section &crit

	images_counted = true;

	card_ptr_t* card_ptrs = NULL;

#ifdef SHANDALAR_SDL_WIN32_COMPAT
	card_ptrs = cards_ptr_shandalar_dll;
	if (!card_ptrs)
		return;
#else
	HMODULE parent = GetModuleHandle(0);

	if (GetProcAddress(parent, "szDeckName")) { //manalink
		card_ptrs = *((card_ptr_t**)(0x73bae0));
	} else if (GetModuleHandle("deck.exe")) { //deck.exe
		// No need to do anything; deckbuilder always shows first image only
		return;
	} else { // shandalar
		if (!cards_ptr_shandalar_dll)
			return;
		card_ptrs = cards_ptr_shandalar_dll;
	}
#endif

	if (idToName == NULL)
		load_names();

	if (!idToName)
		return;
	std::unordered_map<std::string, int> name_to_csvid;
	char buf[200];	// Manalink itself will crash for names much shorter than this
	for (unsigned int i = 0; i < numberOfCards; ++i) {
		const char* name = idToName[i];
		if (!name) continue;
		for (char* p = buf; (*p = tolower(*name)); ++p, ++name){}
		name_to_csvid[buf] = i;
		card_ptrs[i].num_pics = 1;	// Always assume at least one image; it would have been corrected to 1 if initially set to 0 anyway
	}

	ShandalarFileList art_files;
	if (!shandalar_list_files("CardArtManalink/*.jpg", &art_files)) return;
	for (size_t index = 0; index < art_files.count; ++index) {
		try {
			std::string filename = art_files.names[index];

			/* boost::regex would be more concise, but this is about 10% faster when not disk-bound, and more importantly, doesn't bloat the dll file size.
			 * boost::filesystem already increases it from 328k to 1040k; adding boost::regex ballooned it to 1540k. */
			char cardname[200];
			const char* name = filename.c_str();
			unsigned int num = 0;
			for (char* p = cardname; (*p = tolower(*name)); ++p, ++name){
				if (*p == '('){
					if (p > cardname && *(name - 1) == ' ' && *(name + 1) >= '1' && *(name + 1) <= '9'){
						*(p - 1) = 0;	// overwriting the last space before the parenthesis
						++name;
						num = atoi(name);
						while (*name && *name >= '0' && *name <= '9')
							++name;
						if (strcmp(name, ").jpg"))
							num = 0;	// didn't match epilogue
						break;
					} else
						break;	// ( found, but not after a space, or not followed by [1..9]
				}
			}
			if (num == 0)	// no image number found, or failed after finding (
				continue;

			std::unordered_map<std::string, int>::const_iterator csvidit = name_to_csvid.find(cardname);
			if (csvidit == name_to_csvid.end())
				continue;

			int csvid = csvidit->second;
			++num;	// "Grizzly Bears (3).jpg" is actually the fourth image
			if (card_ptrs[csvid].num_pics < num)
				card_ptrs[csvid].num_pics = num;
		} catch (...) {}
	}
	shandalar_free_file_list(&art_files);
}

void Cardartlib_initialize_for_shandalar(card_data_t* real_cards_data, card_ptr_t* real_cards_ptr)
{
	// Avoid querying Shandalar.dll directly; it may not be present.
	(void)real_cards_data;
	cards_ptr_shandalar_dll = real_cards_ptr;
	count_images();	// during startup instead of waiting for first to display
}

int LoadBigArt(int id, int version, LONG width, int height) {
	EnterCriticalSection(&crit);
	if (!images_counted)
		count_images();
	std::string path = idToNameFun(id, version);
	std::unordered_map<std::string, SDL_Surface*>::const_iterator it = cache->find(path);
	if (it == cache->end()) {
		SDL_IOStream *io = shandalar_open_file_io(path.c_str(), "rb");
		if (!io) {
			LeaveCriticalSection(&crit);
			return 0;
		}
		SDL_Surface* loaded = IMG_Load_IO(io, true);
		SDL_Surface* converted = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_BGRA32) : NULL;
		SDL_DestroySurface(loaded);
		if (!converted) {
			LeaveCriticalSection(&crit);
			return 0;
		}
		(*cache)[path] = converted;
	}
	LeaveCriticalSection(&crit);
	return 1; // 1 if the art is available
}

int LoadSmallArt(int id, int version, LONG width, int height) {
	return LoadBigArt(id, version, width, height);
}

void DestroyBigArt(int id, int version) {
	EnterCriticalSection(&crit);
	std::string path = idToNameFun(id, version);
	std::unordered_map<std::string, SDL_Surface*>::iterator it = cache->find(path);
	if (it != cache->end()) {
		SDL_DestroySurface(it->second);
		cache->erase(it);
		//MessageBox(0, path.c_str(), 0, MB_ICONSTOP | MB_SYSTEMMODAL);
	}
	LeaveCriticalSection(&crit);
} //Shandalar

void DestroySmallArt(int id, int version) {
	DestroyBigArt(id, version);
} //Shandalar

int IsBigArtRightSize(int id, int version, int width, int height) {
	return LoadBigArt(id, version, width, height);
}

int IsSmallArtRightSize(int id, int version, int width, int height) {
	return LoadBigArt(id, version, width, height);
}

void DestroyAllBigArts(void) {
	EnterCriticalSection(&crit);
	for (std::unordered_map<std::string, SDL_Surface*>::iterator it = cache->begin(); it != cache->end(); ++it)
		SDL_DestroySurface(it->second);
	cache->clear();
	LeaveCriticalSection(&crit);
} //Shandalar

void DestroyAllSmallArts(void) {
	DestroyAllBigArts();
} //Shandalar

int IsBigArtIn(int id, int version) {
	return 0;
}

int IsSmallArtIn(int id, int version) {
	return 0;
}

int ReloadBigArtIfWrongSize(int id, int version, LONG width, int height) {
	return 0;
} //Shandalar

int ReloadSmallArtIfWrongSize(int id, int version, int width, int height) {
	return 0;
}

int DrawBigArt(HDC hdc, const RECT* rect, int id, int version) {
	if (!LoadBigArt(id, version, 0, 0)) {
		EnterCriticalSection(&crit);
		HBRUSH brush = CreateSolidBrush(RGB(211, 211, 211));
		FillRect(hdc, rect, brush);
		DeleteObject(brush);
		LeaveCriticalSection(&crit);
		return 0;
	}
	EnterCriticalSection(&crit);
	std::string path = idToNameFun(id, version);
	SDL_Surface* surface = (*cache)[path];
	shandalar_dc_blit_bgra32(hdc, rect->left, rect->top,
						   rect->right - rect->left, rect->bottom - rect->top,
						   surface->pixels, surface->pitch, surface->w, surface->h,
						   0, 0, surface->w, surface->h, 255);

	LeaveCriticalSection(&crit);
	return 1;
}

int DrawSmallArt(HDC hdc, const RECT* rect, int id, int version) {
	return DrawBigArt(hdc, rect, id, version);
}

static void init() {
	cache = new std::unordered_map<std::string, SDL_Surface*>;

	InitializeCriticalSection(&crit);

	images_counted = false;
}

static void deinit() {
	delete cache;
	if (idToName) {
		for (unsigned int i = 0; i < numberOfCards; ++i)
			if (idToName[i])
				free(idToName[i]);
		free(idToName);
	}

	DeleteCriticalSection(&crit);
}

int WINAPI DllMain(HINSTANCE hDllHandle, DWORD nReason, LPVOID reserved) {
	switch (nReason)
	{
	case DLL_PROCESS_ATTACH:
		init();
		return TRUE;
		break;
	case DLL_PROCESS_DETACH:
		DestroyAllBigArts();
		DestroyAllSmallArts();
		deinit();
		return TRUE;
		break;
	default:
		return TRUE;
	}
}
