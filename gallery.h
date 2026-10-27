#pragma once

// Pokedex gallery: a filtered grid of thumbnails (All / Raised / Caught / Shiny)
// and the detail view of one species with its large sprite.

#pragma region Includes
#include <Arduino.h>
#pragma endregion

#pragma region Public API
void renderGallery();
// refills galleryFilteredDex for the active filter
void rebuildGalleryFilteredList();
// pages of GALLERY_PAGE_SIZE over the filtered list; an empty filter still shows one page
int galleryPageCount();
// opens the gallery on its first page (Pokedex button on the main screen)
void openGallery();
// closes the gallery and returns to the main screen it is opened from
void closeGalleryToMainScreen();
void galleryTap(int16_t x, int16_t y);
#pragma endregion
