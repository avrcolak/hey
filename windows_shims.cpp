#include "windows_shims.h"

void InflateRect(RECT* lprc, int dx, int dy)
{
	lprc->left -= dx;
	lprc->top -= dy;
	lprc->right += dx;
	lprc->bottom += dy;
}
