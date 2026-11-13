/* reprobe - does a RichEdit class accept the RTF pictures our parser emits?
 * Test only. Usage: reprobe file.rtf. Prints object count per class. */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <richedit.h>
#include <richole.h>


/* minimal IRichEditOleCallback: only GetNewStorage does real work */
typedef struct { IRichEditOleCallback iface; LONG ref; } TtReCb;
static HRESULT STDMETHODCALLTYPE cb_qi(IRichEditOleCallback *t, REFIID r, void **o)
{ if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IRichEditOleCallback)) { *o = t; t->lpVtbl->AddRef(t); return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE cb_addref(IRichEditOleCallback *t) { return ++((TtReCb*)t)->ref; }
static ULONG STDMETHODCALLTYPE cb_release(IRichEditOleCallback *t) { return --((TtReCb*)t)->ref; }
static HRESULT STDMETHODCALLTYPE cb_newstg(IRichEditOleCallback *t, LPSTORAGE *stg)
{ ILockBytes *lb; HRESULT hr = CreateILockBytesOnHGlobal(NULL, TRUE, &lb); if (FAILED(hr)) return hr;
  hr = StgCreateDocfileOnILockBytes(lb, STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_READWRITE, 0, stg); lb->lpVtbl->Release(lb); return hr; }
static HRESULT STDMETHODCALLTYPE cb_inplace(IRichEditOleCallback *t, LPOLEINPLACEFRAME *f, LPOLEINPLACEUIWINDOW *d, LPOLEINPLACEFRAMEINFO i) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE cb_showctx(IRichEditOleCallback *t, BOOL s) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE cb_queryins(IRichEditOleCallback *t, LPCLSID c, LPSTORAGE s, LONG cp) { return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_delobj(IRichEditOleCallback *t, LPOLEOBJECT o) { return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_qacc(IRichEditOleCallback *t, LPDATAOBJECT d, CLIPFORMAT *f, DWORD r, BOOL rl, HGLOBAL m) { return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_ctxsens(IRichEditOleCallback *t, BOOL e) { return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_clip(IRichEditOleCallback *t, CHARRANGE *c, DWORD r, LPDATAOBJECT *d) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE cb_drag(IRichEditOleCallback *t, BOOL dr, DWORD k, LPDWORD e) { return S_OK; }
static HRESULT STDMETHODCALLTYPE cb_menu(IRichEditOleCallback *t, WORD s, LPOLEOBJECT o, CHARRANGE *c, HMENU *m) { return E_NOTIMPL; }
static IRichEditOleCallbackVtbl cb_vtbl = { cb_qi, cb_addref, cb_release, cb_newstg, cb_inplace, cb_showctx,
    cb_queryins, cb_delobj, cb_qacc, cb_ctxsens, cb_clip, cb_drag, cb_menu };
static TtReCb the_cb = { { &cb_vtbl }, 1 };

static char *buf; static long len, pos;
static DWORD CALLBACK cb(DWORD_PTR c, BYTE *b, LONG n, LONG *got)
{ long k = len - pos; if (k > n) k = n; memcpy(b, buf + pos, k); pos += k; *got = k; return 0; }

static void probe(const char *cls)
{
    HWND re = CreateWindowExA(0, cls, NULL, ES_MULTILINE | WS_POPUP, 0, 0, 600, 400, NULL, NULL, NULL, NULL);
    EDITSTREAM es = { 0, 0, cb };
    IRichEditOle *ole = NULL;
    if (!re) { printf("%-12s no window (%lu)\n", cls, GetLastError()); return; }
    if (getenv("CB")) SendMessageA(re, EM_SETOLECALLBACK, 0, (LPARAM)&the_cb.iface);
    pos = 0;
    SendMessageA(re, EM_STREAMIN, SF_RTF, (LPARAM)&es);
    SendMessageA(re, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    printf("%-12s chars=%d objects=%ld err=%lu\n", cls, GetWindowTextLengthA(re),
           ole ? ole->lpVtbl->GetObjectCount(ole) : -1L, es.dwError);
    DestroyWindow(re);
}

int gen_main(void);
int main(int argc, char **argv)
{
    if (!strcmp(argv[1], "--gen")) return gen_main();
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    buf = malloc(len); fread(buf, 1, len, f); fclose(f);
    printf("OleInitialize=%lx\n", OleInitialize(NULL));
    LoadLibraryA("riched20.dll"); LoadLibraryA("msftedit.dll");
    probe("RichEdit20A"); probe("RICHEDIT50W");
    return 0;
}

/* "reprobe --gen": build candidate RTF picture encodings of one 4x4 DIB and probe each */
static char genbuf[65536];
static void hexcat(char *d, const BYTE *p, int n) { d += strlen(d); while (n--) d += sprintf(d, "%02x", *p++); }
int gen_main(void)
{
    struct { BITMAPINFOHEADER h; RGBQUAD c[2]; } bi = { { sizeof(BITMAPINFOHEADER), 4, 4, 1, 24, BI_RGB, 0, 0, 0, 0, 0 } };
    BYTE bits[4 * 12]; int i; HDC dc; UINT sz; BYTE *mf;
    for (i = 0; i < 48; i += 3) { bits[i] = 0; bits[i+1] = 0; bits[i+2] = 255; }
    OleInitialize(NULL); LoadLibraryA("riched20.dll"); LoadLibraryA("msftedit.dll");
    /* 1. WMF */
    dc = CreateMetaFileA(NULL);
    SetMapMode(dc, MM_ANISOTROPIC);   /* harmless in WMF */
    StretchDIBits(dc, 0, 0, 4, 4, 0, 0, 4, 4, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS, SRCCOPY);
    { HMETAFILE m = CloseMetaFile(dc); sz = GetMetaFileBitsEx(m, 0, NULL); mf = malloc(sz); GetMetaFileBitsEx(m, sz, mf); }
    strcpy(genbuf, "{\\rtf1 A{\\pict\\wmetafile8\\picw4\\pich4\\picwgoal60\\pichgoal60 "); hexcat(genbuf, mf, sz); strcat(genbuf, "}B}");
    buf = genbuf; len = strlen(genbuf); printf("[wmetafile8]\n"); probe("RichEdit20A"); probe("RICHEDIT50W");
    /* 2. dibitmap0 */
    strcpy(genbuf, "{\\rtf1 A{\\pict\\dibitmap0\\picw4\\pich4\\picwgoal60\\pichgoal60 ");
    hexcat(genbuf, (BYTE*)&bi.h, sizeof(bi.h)); hexcat(genbuf, bits, 48); strcat(genbuf, "}B}");
    buf = genbuf; len = strlen(genbuf); printf("[dibitmap0]\n"); probe("RichEdit20A"); probe("RICHEDIT50W");
    return 0;
}
