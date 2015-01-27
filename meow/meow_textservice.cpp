#include "meow.h"
#include "meow_window.h"
#include "meow_textservice.h"
#include "meow_composition.h"

MeowTextService::MeowTextService() {
	reference = 1;
	threadmgr = NULL;

	_dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
	_dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
	_dwTextEditSinkCookie = TF_INVALID_COOKIE;

	windowmanager  = new MeowWindowManager(Meow::hinstance);
	Meow::DllAddRef();
}
MeowTextService::~MeowTextService() {
	delete windowmanager;
	Meow::DllRelease();
}

HRESULT MeowTextService::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObj) {
	MeowTextService * tmp = nullptr;
	HRESULT hr = S_OK;

	if (ppvObj == nullptr) return E_INVALIDARG;
	*ppvObj = nullptr;
	if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

	tmp = new MeowTextService();
	if (tmp == NULL) return E_OUTOFMEMORY;
	hr = tmp->QueryInterface(riid, ppvObj);
	tmp->Release(); // caller still holds ref if hr == S_OK
	return hr;
}
HRESULT STDMETHODCALLTYPE MeowTextService::QueryInterface(REFIID riid, void **ppvObj) {
	if (ppvObj == NULL) return E_INVALIDARG;
	*ppvObj = NULL;

	if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor))
	{
		Meow::DebugLog("QUERY IID_ITfTextInputProcessor");
		*ppvObj = (ITfTextInputProcessor *)this;
	}
	else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
	{
		Meow::DebugLog("QUERY IID_ITfTextInputProcessorEx");
		*ppvObj = (ITfTextInputProcessorEx *)this;
	}
	else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
	{
		Meow::DebugLog("QUERY IID_ITfThreadMgrEventSink");
		*ppvObj = (ITfThreadMgrEventSink *)this;
	}
	else if (IsEqualIID(riid, IID_ITfThreadFocusSink)) {
		Meow::DebugLog("QUERY IID_ITfThreadFocusSink");
		*ppvObj = (ITfThreadFocusSink *)this;
	}
	else if (IsEqualIID(riid, IID_ITfKeyEventSink))
	{
		Meow::DebugLog("QUERY IID_ITfKeyEventSink");
		*ppvObj = (ITfKeyEventSink *)this;
	}
	else if (IsEqualIID(riid, IID_ITfCompositionSink))
	{
		Meow::DebugLog("QUERY IID_ITfCompositionSink");
		*ppvObj = (ITfCompositionSink *)this->compositionmanager;
	}
	else if (IsEqualIID(riid, IID_ITfTextEditSink))
	{
		Meow::DebugLog("QUERY IID_ITfTextEditSink");
		// better use compositionmanager
		*ppvObj = (ITfTextEditSink *)this;
	}
	else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
	{
		Meow::DebugLog("QUERY ITfDisplayAttributeProvider");
		*ppvObj = (ITfDisplayAttributeProvider *)this;
	}
	else if (IsEqualIID(riid, IID_ITfDisplayAttributeCollectionProvider))
	{
		Meow::DebugLog("QUERY ITfDisplayAttributeCollectionProvider");
		//*ppvObj = (ITfDisplayAttributeProvider *)this;
	}
	else {
		LPOLESTR str;
		StringFromIID(riid, &str);
		_bstr_t bs(str, false);
		CHAR * xx = bs;
		Meow::DebugError(bs);
	}

	if (*ppvObj)
	{
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MeowTextService::AddRef() {
	return ++reference;
}
ULONG STDMETHODCALLTYPE MeowTextService::Release() {
	if (--reference <= 0) delete this;
	return reference;
}

HRESULT STDMETHODCALLTYPE MeowTextService::ActivateEx(ITfThreadMgr *pThreadMgr, TfClientId tfClientId, DWORD dwFlags) {
	threadmgr = pThreadMgr;
	clientid = tfClientId;
	threadmgr->AddRef();
	flags = dwFlags;
	Meow::DebugVal(flags);

	compositionmanager = new MeowCompositionManager(clientid, this);

	if (!_InitThreadMgrEventSink())
		goto ExitError;


	ITfDocumentMgr *pDocMgrFocus;
	HRESULT hr = NULL;
	hr = threadmgr->GetFocus(&pDocMgrFocus);
	if (hr == S_OK) {
		Meow::DebugLog("GetFocus OK");
	}
	else {

	}
	Meow::DebugVal((DWORD)hr);
	if ((hr == S_OK) &&
		(pDocMgrFocus != NULL))
	{
		Meow::DebugLog("GOING _InitTextEditSink");
		_InitTextEditSink(pDocMgrFocus);
		pDocMgrFocus->Release();
	}
	BOOL foc;
	hr = threadmgr->IsThreadFocus(&foc);
	if (hr == S_OK) {
		if (foc) Meow::DebugLog("IsThreadFocus TRUE");
		else Meow::DebugLog("IsThreadFocus NO");
	}

	//threadmgr->CreateDocumentMgr(&pDocMgrFocus);


	if (!_InitThreadFocusSink())
		goto ExitError;

	if (!_InitKeyEventSink())
		goto ExitError;

	if (!_InitPreservedKey())
		goto ExitError;

	if (!_InitDisplayAttributeGuidAtom())
	{
		goto ExitError;
	}

	Meow::DebugLog("Activate SUCCESS");
	return S_OK;
ExitError:
	Deactivate(); // cleanup any half-finished init
	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE MeowTextService::Activate(ITfThreadMgr *pThreadMgr, TfClientId tfClientId) {
	return ActivateEx(pThreadMgr, tfClientId, 0);
	Meow::DebugLog("Activate No Ex");
}
HRESULT STDMETHODCALLTYPE MeowTextService::Deactivate() {


	_UninitPreservedKey();
	_UninitKeyEventSink();


	_UninitThreadFocusSink();
	 _UninitThreadMgrEventSink();
	// Release ALL refs to threadmgr in Deactivate



	 delete compositionmanager;

	if (threadmgr != NULL)
	{
		threadmgr->Release();
		threadmgr = NULL;
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE MeowTextService::OnSetThreadFocus()
{
	Meow::DebugLog("OnSetThreadFocus");
	windowmanager->ShowStatusWindow();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE MeowTextService::OnKillThreadFocus()
{
	Meow::DebugLog("OnKillThreadFocus");
	windowmanager->HideStatusWindow();
	return S_OK;
}

BOOL MeowTextService::_InitThreadFocusSink()
{
	ITfSource* pSource = nullptr;

	if (FAILED(threadmgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
	{
		return FALSE;
	}

	if (FAILED(pSource->AdviseSink(IID_ITfThreadFocusSink, (ITfThreadFocusSink *)this, &_dwThreadFocusSinkCookie)))
	{
		pSource->Release();
		return FALSE;
	}

	pSource->Release();

	return TRUE;
}

VOID MeowTextService::_UninitThreadFocusSink()
{
	ITfSource* pSource = nullptr;


	if (FAILED(threadmgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
	{
		return;
	}


	if (FAILED(pSource->UnadviseSink(_dwThreadFocusSinkCookie)))
	{
		pSource->Release();
		return;
	}

	pSource->Release();
}

STDAPI MeowTextService::OnInitDocumentMgr(ITfDocumentMgr *pDocMgr)
{
	Meow::DebugLog("OnInitDocumentMgr");
	return S_OK;
}

STDAPI MeowTextService::OnUninitDocumentMgr(ITfDocumentMgr *pDocMgr)
{
	Meow::DebugLog("OnUninitDocumentMgr");
	return S_OK;
}

STDAPI MeowTextService::OnSetFocus(ITfDocumentMgr *pDocMgrFocus, ITfDocumentMgr *pDocMgrPrevFocus)
{


	compositionmanager->Switch(pDocMgrFocus, pDocMgrPrevFocus);

	Meow::DebugLog("CTextService::OnSetFocus ITfDocumentMgr");
	Meow::DebugLog("GOING _InitTextEditSink");
	_InitTextEditSink(pDocMgrFocus);
	return S_OK;
}

STDAPI MeowTextService::OnPushContext(ITfContext *pContext)
{
	Meow::DebugLog("OnPushContext");
	return S_OK;
}
STDAPI MeowTextService::OnPopContext(ITfContext *pContext)
{
	Meow::DebugLog("OnPopContext");
	return S_OK;
}

BOOL MeowTextService::_InitThreadMgrEventSink()
{
	ITfSource *pSource;
	BOOL fRet;

	if (threadmgr->QueryInterface(IID_ITfSource, (void **)&pSource) != S_OK) {
		return FALSE;
	}

	fRet = FALSE;

	if (pSource->AdviseSink(IID_ITfThreadMgrEventSink, (ITfThreadMgrEventSink *)this, &_dwThreadMgrEventSinkCookie) != S_OK)
	{
		// Don't try to Unadvise _dwThreadMgrEventSinkCookie later
		_dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
		goto Exit;
	} 

	fRet = TRUE;

Exit:
	pSource->Release();
	return fRet;
}
void MeowTextService::_UninitThreadMgrEventSink()
{
	ITfSource *pSource;

	if (_dwThreadMgrEventSinkCookie == TF_INVALID_COOKIE) {
		return; // never Advised
	}

	if (threadmgr->QueryInterface(IID_ITfSource, (void **)&pSource) == S_OK)
	{
		pSource->UnadviseSink(_dwThreadMgrEventSinkCookie);
		pSource->Release();
	}

	_dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
}




STDAPI MeowTextService::OnEndEdit(ITfContext *pContext, TfEditCookie ecReadOnly, ITfEditRecord *pEditRecord)
{
	// FIXME: should register compositionmanager as ITfTextEditSink
	compositionmanager->OnEndEdit(pContext, ecReadOnly, pEditRecord);
	return S_OK;
}

//+---------------------------------------------------------------------------
//
// _InitTextEditSink
//
// Init a text edit sink on the topmost context of the document.
// Always release any previous sink.
//----------------------------------------------------------------------------

BOOL MeowTextService::_InitTextEditSink(ITfDocumentMgr *pDocMgr)
{
	ITfSource *pSource;
	BOOL fRet;

	// clear out any previous sink first

	Meow::DebugLog("_InitTextEditSink 1");
	if (_dwTextEditSinkCookie != TF_INVALID_COOKIE)
	{
		Meow::DebugLog("_InitTextEditSink 2");
		if (_pTextEditSinkContext->QueryInterface(IID_ITfSource, (void **)&pSource) == S_OK)
		{

			Meow::DebugLog("_InitTextEditSink 3");
			pSource->UnadviseSink(_dwTextEditSinkCookie);
			pSource->Release();
		}

		Meow::DebugLog("_InitTextEditSink 4");
		_pTextEditSinkContext->Release();
		_pTextEditSinkContext = NULL;
		_dwTextEditSinkCookie = TF_INVALID_COOKIE;
	}

	if (pDocMgr == NULL)
	{

		Meow::DebugLog("_InitTextEditSink 5");
		return TRUE; // caller just wanted to clear the previous sink
	}

	// setup a new sink advised to the topmost context of the document

	if (pDocMgr->GetTop(&_pTextEditSinkContext) != S_OK) {

		Meow::DebugLog("_InitTextEditSink 6");
		return FALSE;
	}

	if (_pTextEditSinkContext == NULL) {

		Meow::DebugLog("_InitTextEditSink 7");
		return TRUE; // empty document, no sink possible
	}

	fRet = FALSE;

	if (_pTextEditSinkContext->QueryInterface(IID_ITfSource, (void **)&pSource) == S_OK)
	{
		Meow::DebugLog("_InitTextEditSink 8");
		if (pSource->AdviseSink(IID_ITfTextEditSink, (ITfTextEditSink *)this, &_dwTextEditSinkCookie) == S_OK)
		{
			Meow::DebugLog("_InitTextEditSink 9");
			fRet = TRUE;
		}
		else
		{
			Meow::DebugLog("_InitTextEditSink 10");
			_dwTextEditSinkCookie = TF_INVALID_COOKIE;
		}
		pSource->Release();
	}

	Meow::DebugLog("_InitTextEditSink 11");
	if (fRet == FALSE)
	{
		Meow::DebugLog("_InitTextEditSink 12");
		_pTextEditSinkContext->Release();
		_pTextEditSinkContext = NULL;
	}

	Meow::DebugLog("_InitTextEditSink 13");
	return fRet;
}





STDAPI MeowTextService::OnSetFocus(BOOL fForeground)
{
	Meow::DebugLog("MeowTextService::OnSetFocus fForeground");
	// may need to store and resume composition here

	ITfDocumentMgr *pDocMgrFocus;
	if ((threadmgr->GetFocus(&pDocMgrFocus) == S_OK) &&
		(pDocMgrFocus != NULL))
	{
		Meow::DebugLog("GOING _InitTextEditSink");
		_InitTextEditSink(pDocMgrFocus);
		pDocMgrFocus->Release();
	}
	Meow::DebugLog("MeowTextService::OnSetFocus FINISH");
	return S_OK;
}

STDAPI MeowTextService::OnTestKeyDown(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
	Meow::DebugLog("MeowTextService::OnTestKeyDown");
	*pfEaten = compositionmanager->OnTestKeyDown(pContext, wParam);
	return S_OK;
}

STDAPI MeowTextService::OnTestKeyUp(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
	*pfEaten = compositionmanager->OnTestKeyUp(pContext, wParam);
	return S_OK;
}

STDAPI MeowTextService::OnKeyDown(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
	Meow::DebugLog("MeowTextService::OnKeyDown");
	*pfEaten = compositionmanager->OnKeyDown(pContext, wParam);
	return S_OK;
}

STDAPI MeowTextService::OnKeyUp(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
	*pfEaten = compositionmanager->OnKeyUp(pContext, wParam);
	return S_OK;
}

STDAPI MeowTextService::OnPreservedKey(ITfContext *pContext, REFGUID rguid, BOOL *pfEaten)
{
	/*
	if (IsEqualGUID(rguid, GUID_PRESERVEDKEY_ONOFF))
	{
		BOOL fOpen = _IsKeyboardOpen();
		_SetKeyboardOpen(fOpen ? FALSE : TRUE);
		*pfEaten = TRUE;
	}
	else
	{
		*pfEaten = FALSE;
	}
	*/
	return S_OK;
}

//+---------------------------------------------------------------------------
//
// _InitKeyEventSink
//
// Advise a keystroke sink.
//----------------------------------------------------------------------------

BOOL MeowTextService::_InitKeyEventSink()
{
	ITfKeystrokeMgr *pKeystrokeMgr;
	HRESULT hr;

	if (threadmgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr) != S_OK) {
		Meow::DebugLog("IID_ITfKeystrokeMgr FAILED");
		return FALSE;
	}

	hr = pKeystrokeMgr->AdviseKeyEventSink(clientid, (ITfKeyEventSink *)this, TRUE);
	if (hr != S_OK) {
		Meow::DebugLog("AdviseKeyEventSink FAILED 1");
	}
	pKeystrokeMgr->Release();

	return (hr == S_OK);
}

//+---------------------------------------------------------------------------
//
// _UninitKeyEventSink
//
// Unadvise a keystroke sink.  Assumes a sink has been advised already.
//----------------------------------------------------------------------------

void MeowTextService::_UninitKeyEventSink()
{
	ITfKeystrokeMgr *pKeystrokeMgr;

	if (threadmgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr) != S_OK)
		return;

	pKeystrokeMgr->UnadviseKeyEventSink(clientid);

	pKeystrokeMgr->Release();
}

//+---------------------------------------------------------------------------
//
// _InitPreservedKey
//
// Register a hot key.
//----------------------------------------------------------------------------

BOOL MeowTextService::_InitPreservedKey()
{
	ITfKeystrokeMgr *pKeystrokeMgr;

	if (threadmgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr) != S_OK)
		return FALSE;
	/*
	HRESULT hr;
	// register Alt+~ key
	hr = pKeystrokeMgr->PreserveKey(_tfClientId,
		GUID_PRESERVEDKEY_ONOFF,
		&c_pkeyOnOff0,
		c_szPKeyOnOff,
		wcslen(c_szPKeyOnOff));

	// register KANJI key
	hr = pKeystrokeMgr->PreserveKey(_tfClientId,
		GUID_PRESERVEDKEY_ONOFF,
		&c_pkeyOnOff1,
		c_szPKeyOnOff,
		wcslen(c_szPKeyOnOff));

	// register F6 key
	hr = pKeystrokeMgr->PreserveKey(_tfClientId,
		GUID_PRESERVEDKEY_F6,
		&c_pkeyF6,
		c_szPKeyF6,
		wcslen(c_szPKeyF6));
		*/
	pKeystrokeMgr->Release();

	return TRUE;
}

//+---------------------------------------------------------------------------
//
// _UninitPreservedKey
//
// Uninit a hot key.
//----------------------------------------------------------------------------

void MeowTextService::_UninitPreservedKey()
{
	ITfKeystrokeMgr *pKeystrokeMgr;

	if (threadmgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr) != S_OK)
		return;

	//pKeystrokeMgr->UnpreserveKey(GUID_PRESERVEDKEY_ONOFF, &c_pkeyOnOff0);
	//pKeystrokeMgr->UnpreserveKey(GUID_PRESERVEDKEY_ONOFF, &c_pkeyOnOff1);
	//pKeystrokeMgr->UnpreserveKey(GUID_PRESERVEDKEY_F6, &c_pkeyF6);

	pKeystrokeMgr->Release();
}

BOOL MeowTextService::_InitDisplayAttributeGuidAtom()
{
	// FIXME: COMLESS MODE?
	ITfCategoryMgr* pCategoryMgr = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&pCategoryMgr);

	if (FAILED(hr))
	{
		return FALSE;
	}

	// FIXME: GUID_DISPLAY_ARRTIBUTE_INFO unregister
	// register the display attribute for input text.
	hr = pCategoryMgr->RegisterGUID(Meow::GUID_DISPLAY_ARRTIBUTE_INFO, &displayattribute);
	if (FAILED(hr))
	{
		goto Exit;
	}

Exit:
	pCategoryMgr->Release();

	return (hr == S_OK);
}



STDAPI MeowTextService::EnumDisplayAttributeInfo(__RPC__deref_out_opt IEnumTfDisplayAttributeInfo **ppEnum)
{
	if (ppEnum == nullptr)
	{
		return E_INVALIDARG;
	}
	*ppEnum = nullptr;
	*ppEnum = new (std::nothrow) MeowDisplayAttributeInfoEnum();
	if (*ppEnum == nullptr)
	{
		return E_OUTOFMEMORY;
	}
	return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfDisplayAttributeProvider::GetDisplayAttributeInfo
//
//----------------------------------------------------------------------------

STDAPI MeowTextService::GetDisplayAttributeInfo(__RPC__in REFGUID guidInfo, __RPC__deref_out_opt ITfDisplayAttributeInfo **ppInfo)
{
	if (ppInfo == nullptr)
	{
		return E_INVALIDARG;
	}
	*ppInfo = nullptr;
	if (IsEqualGUID(guidInfo, Meow::GUID_DISPLAY_ARRTIBUTE_INFO))
	{
		*ppInfo = new (std::nothrow) MeowDisplayAttributeInfo();
		if ((*ppInfo) == nullptr)
		{
			return E_OUTOFMEMORY;
		}
	}
	else
	{
		return E_INVALIDARG;
	}
	return S_OK;
}