//------------------------------------------------------------------------------
// File: StillCapDlg.cpp
//
// Desc: DirectShow sample code - implementation of callback and dialog
//       objects for StillCap application.
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------


#include "stdafx.h"
#include "StillCap.h"
#include "StillCapDlg.h"
#include "dshowutil.cpp"
#include "dshow.h"
#include "ks.h"
#include "ksmedia.h"
#include ".\stillcapdlg.h"
#include <stdio.h>
#include <string.h>
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// An application can advertise the existence of its filter graph
// by registering the graph with a global Running Object Table (ROT).
// The GraphEdit application can detect and remotely view the running
// filter graph, allowing you to 'spy' on the graph with GraphEdit.
//
// To enable registration in this sample, define REGISTER_FILTERGRAPH.
//
#ifdef DEBUG
#define REGISTER_FILTERGRAPH
#endif

// Constants
#define WM_CAPTURE_BITMAP   WM_APP + 1

// Global data
BOOL g_bOneShot=FALSE;
DWORD g_dwGraphRegister=0;  // For running object table
HWND g_hwnd=0;
HWND ghwndApp=0;
CComPtr< IBaseFilter > pCap;
CComPtr<ICaptureGraphBuilder2> pCGB2;
IAMTVTuner *pTV;
ISpecifyPropertyPages *pSpec;
CAUUID cauuid;
TCHAR  buffer[MAX_PATH];
CDialogBar m_wndDlgBar;
int l =1,m=0;
int k =87000000;
int z=0;
int smode=0;
int max_frq=855200000;
int min_frq=48300000;
// Structures
typedef struct _callbackinfo 
{
    double dblSampleTime;
    long lBufferSize;
    BYTE *pBuffer;
    BITMAPINFOHEADER bih;

} CALLBACKINFO;

CALLBACKINFO cb={0};


// Note: this object is a SEMI-COM object, and can only be created statically.
// We use this little semi-com object to handle the sample-grab-callback,
// since the callback must provide a COM interface. We could have had an interface
// where you provided a function-call callback, but that's really messy, so we
// did it this way. You can put anything you want into this C++ object, even
// a pointer to a CDialog. Be aware of multi-thread issues though.
//
class CSampleGrabberCB : public ISampleGrabberCB 
{
public:
    // these will get set by the main thread below. We need to
    // know this in order to write out the bmp
    long lWidth;
    long lHeight;
    CStillCapDlg * pOwner;
    TCHAR m_szCapDir[MAX_PATH]; // the directory we want to capture to
    TCHAR m_szSnappedName[MAX_PATH];
    BOOL bFileWritten;

    CSampleGrabberCB( )
    {
        pOwner = NULL;
        ZeroMemory(m_szCapDir, sizeof(m_szCapDir));
        ZeroMemory(m_szSnappedName, sizeof(m_szSnappedName));
        bFileWritten = FALSE;
    }   

    // fake out any COM ref counting
    //
    STDMETHODIMP_(ULONG) AddRef() { return 2; }
    STDMETHODIMP_(ULONG) Release() { return 1; }

    // fake out any COM QI'ing
    //
    STDMETHODIMP QueryInterface(REFIID riid, void ** ppv)
    {
        if( riid == IID_ISampleGrabberCB || riid == IID_IUnknown ) 
        {
            *ppv = (void *) static_cast<ISampleGrabberCB*> ( this );
            return NOERROR;
        }    
        return E_NOINTERFACE;
    }

    // we don't implement this interface for this example
    //
    STDMETHODIMP SampleCB( double SampleTime, IMediaSample * pSample )
    {
        return 0;
    }

    // The sample grabber is calling us back on its deliver thread.
    // This is NOT the main app thread!
    //
    //           !!!!! WARNING WARNING WARNING !!!!!
    //
    // On Windows 9x systems, you are not allowed to call most of the 
    // Windows API functions in this callback.  Why not?  Because the
    // video renderer might hold the global Win16 lock so that the video
    // surface can be locked while you copy its data.  This is not an
    // issue on Windows 2000, but is a limitation on Win95,98,98SE, and ME.
    // Calling a 16-bit legacy function could lock the system, because 
    // it would wait forever for the Win16 lock, which would be forever
    // held by the video renderer.
    //
    // As a workaround, copy the bitmap data during the callback,
    // post a message to our app, and write the data later.
    //
    STDMETHODIMP BufferCB( double dblSampleTime, BYTE * pBuffer, long lBufferSize )
    {
        // this flag will get set to true in order to take a picture
        //
        if( !g_bOneShot )
            return 0;

        if (!pBuffer)
            return E_POINTER;

        if( cb.lBufferSize < lBufferSize )
        {
            delete [] cb.pBuffer;
            cb.pBuffer = NULL;
            cb.lBufferSize = 0;
        }

        // Since we can't access Windows API functions in this callback, just
        // copy the bitmap data to a global structure for later reference.
        cb.dblSampleTime = dblSampleTime;

        // If we haven't yet allocated the data buffer, do it now.
        // Just allocate what we need to store the new bitmap.
        if (!cb.pBuffer)
        {
            cb.pBuffer = new BYTE[lBufferSize];
            cb.lBufferSize = lBufferSize;
        }

        if( !cb.pBuffer )
        {
            cb.lBufferSize = 0;
            return E_OUTOFMEMORY;
        }

        // Copy the bitmap data into our global buffer
        memcpy(cb.pBuffer, pBuffer, lBufferSize);

        // Post a message to our application, telling it to come back
        // and write the saved data to a bitmap file on the user's disk.
        PostMessage(g_hwnd, WM_CAPTURE_BITMAP, 0, 0L);
        return 0;
    }
   
};


CSampleGrabberCB mCB;


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
    CAboutDlg();

// Dialog Data
    //{{AFX_DATA(CAboutDlg)
    enum { IDD = IDD_ABOUTBOX };
    //}}AFX_DATA

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CAboutDlg)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

// Implementation
protected:
    //{{AFX_MSG(CAboutDlg)
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
    //{{AFX_DATA_INIT(CAboutDlg)
    //}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CAboutDlg)
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
    //{{AFX_MSG_MAP(CAboutDlg)
        // No message handlers
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CStillCapDlg dialog

CStillCapDlg::CStillCapDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CStillCapDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CStillCapDlg)
        // NOTE: the ClassWizard will add member initialization here
    //}}AFX_DATA_INIT
    // Note that LoadIcon does not require a subsequent DestroyIcon in Win32
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CStillCapDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CStillCapDlg)
	DDX_Control(pDX, IDC_EDIT1, m_freq1);
	DDX_Control(pDX, IDC_EDIT2, m_freq2);
	DDX_Control(pDX, IDC_EDIT3, m_step);
	DDX_Control(pDX, IDC_EDIT4, m_sg);
	DDX_Control(pDX, IDC_STATIC1, m_ST1);
	DDX_Control(pDX, IDC_STATIC2, m_ST2);
    DDX_Control(pDX, IDC_STATUS, m_StrStatus);
    DDX_Control(pDX, IDC_CAPOBJ, m_PreviewScreen);
	DDX_Control(pDX, IDC_MHZ , m_mhz);
	DDX_Control(pDX, IDC_HZ , m_hz);
	DDX_Control(pDX, IDC_HZ2 , m_hz2);
	DDX_Control(pDX, IDC_SG, m_lblsg);
    DDX_Control(pDX, IDC_STEP , m_lblstep);
	DDX_Control(pDX, IDC_BUTTON1, m_button1);
	DDX_Control(pDX, IDC_BUTTON2, m_button2);
	DDX_Control(pDX, IDC_BUTTON3, m_button3);
	DDX_Control(pDX, IDC_BUTTON4, m_button4);
	DDX_Control(pDX, IDC_BUTTON5, m_button5);
	DDX_Control(pDX, IDC_BUTTON6, m_button6);
	DDX_Control(pDX, IDC_BUTTON7, m_button7);
	DDX_Control(pDX, IDC_BUTTON8, m_button8);
	DDX_Control(pDX, IDC_BUTTON9, m_button9);
	DDX_Control(pDX, IDC_BUTTON10, m_button10);
	DDX_Control(pDX, IDC_BUTTON11, m_button11);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CStillCapDlg, CDialog)
    //{{AFX_MSG_MAP(CStillCapDlg)
    ON_WM_PAINT()
    ON_WM_SYSCOMMAND()
    ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
    ON_WM_CLOSE()
    //}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, OnBnClickedButton2)
	ON_BN_CLICKED(IDC_BUTTON3, OnBnClickedButton3)
	ON_BN_CLICKED(IDC_BUTTON4, OnBnClickedButton4)
	ON_BN_CLICKED(IDC_BUTTON5, OnBnClickedButton5)
	ON_BN_CLICKED(IDC_BUTTON6, OnBnClickedButton6)
	ON_BN_CLICKED(IDC_BUTTON7, OnBnClickedButton7)
	ON_BN_CLICKED(IDC_BUTTON8, OnBnClickedButton8)
	ON_BN_CLICKED(IDC_BUTTON10, OnBnClickedButton10)
	ON_BN_CLICKED(IDC_BUTTON11, OnBnClickedButton11)
	ON_BN_CLICKED(IDC_BUTTON9, OnBnClickedButton9)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CStillCapDlg message handlers

void CStillCapDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else
    {
        CDialog::OnSysCommand(nID, lParam);
    }
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.
void CStillCapDlg::OnPaint() 
{
	
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;
        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
			
		}
    else
    {
		if (m==0){
			CClientDC dc(this);                           
			CRect rClient;
			GetClientRect(&rClient);
			dc.FillSolidRect(rClient, RGB(200,200,200));
			m==1;
         }
		m_button1.RedrawWindow(0,0,1);
		m_button2.RedrawWindow(0,0,1);
		m_button3.RedrawWindow(0,0,1);
		m_button4.RedrawWindow(0,0,1);
		m_button5.RedrawWindow(0,0,1);
		m_button6.RedrawWindow(0,0,1);
		m_button7.RedrawWindow(0,0,1);
		m_button8.RedrawWindow(0,0,1);
		m_button9.RedrawWindow(0,0,1);
		m_button10.RedrawWindow(0,0,1);
		m_button11.RedrawWindow(0,0,1);
		m_freq1.RedrawWindow(0,0,1);
		m_freq2.RedrawWindow(0,0,1);
		m_step.RedrawWindow(0,0,1);
		m_sg.RedrawWindow(0,0,1);
		m_StrStatus.RedrawWindow(0,0,1);
		m_PreviewScreen.RedrawWindow(0,0,1);
		m_mhz.RedrawWindow(0,0,1);
		m_hz.RedrawWindow(0,0,1);
		m_hz2.RedrawWindow(0,0,1);
		m_lblsg.RedrawWindow(0,0,1);
		m_lblstep.RedrawWindow(0,0,1);
		m_ST1.RedrawWindow(0,0,1);
		m_ST2.RedrawWindow(0,0,1);
		
    CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CStillCapDlg::OnQueryDragIcon()
{
    return (HCURSOR) m_hIcon;
}

BOOL CStillCapDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
	    // Add "About..." menu item to system menu.

    // IDM_ABOUTBOX must be in the system command range.
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != NULL)
    {
        CString strAboutMenu;
        strAboutMenu.LoadString(IDS_ABOUTBOX);
        if (!strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);         // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon
	m_freq1.SetWindowText("100.00");	
   	m_freq2.SetWindowText("100000000");	
	m_step.SetWindowText("100000");	
	m_sg.SetWindowText("0");	
	m_lblsg.SetBkColor(RGB( 200, 200, 200 ));
	m_lblstep.SetBkColor(RGB( 200, 200, 200 ));
	m_mhz.SetBkColor(RGB( 200, 200, 200 ));
	m_hz.SetBkColor(RGB( 200, 200, 200 ));
	m_hz2.SetBkColor(RGB( 200, 200, 200 ));
    m_StrStatus.SetTextColor(RGB( 0, 0, 200 ));
	m_StrStatus.SetBkColor(RGB( 200, 200, 200 ));
	m_PreviewScreen.SetTextColor(RGB( 50, 50, 50 ));
	m_PreviewScreen.SetBkColor(RGB( 200, 200, 200 ));
	m_button1.SetBkColor(RGB( 90, 150, 190));
	m_button1.SetTextColor(RGB(255, 255, 255 ));
	m_button2.SetBkColor(RGB( 90, 150, 190 ));
	m_button2.SetTextColor(RGB(255, 255, 255 ));
	m_button3.SetBkColor(RGB( 90, 150, 190));
	m_button3.SetTextColor(RGB(255, 255, 255 ));
	m_button5.SetBkColor(RGB( 90, 150, 190));
	m_button5.SetTextColor(RGB(255, 255, 255 ));
	m_button6.SetBkColor(RGB( 90, 150, 190 ));
	m_button6.SetTextColor(RGB(255, 255, 255 ));
	m_button4.SetBkColor(RGB( 50, 150, 50 ));
	m_button4.SetTextColor(RGB(255, 255, 255 ));
	m_button8.SetBkColor(RGB( 50, 150, 50 ));
	m_button8.SetTextColor(RGB(255, 255, 255 ));
	m_button7.SetBkColor(RGB( 50, 150, 50 ));
	m_button7.SetTextColor(RGB(255, 255, 255 ));
	m_button10.SetBkColor(RGB( 50, 150, 50 ));
	m_button10.SetTextColor(RGB(255, 255, 255 ));
	m_button11.SetBkColor(RGB( 50, 150, 50 ));
	m_button11.SetTextColor(RGB(255, 255, 255 ));
	m_button9.SetBkColor(RGB( 90, 150, 190 ));
	m_button9.SetTextColor(RGB(255, 255, 255 ));
	m_freq1.SetBkColor(RGB( 0, 50, 140 ));
	m_freq1.SetTextColor(RGB(255, 255, 255 ));
	m_freq2.SetBkColor(RGB( 0, 50, 140 ));
	m_freq2.SetTextColor(RGB(255, 255, 255 ));
	m_step.SetBkColor(RGB( 0, 50, 140 ));
	m_step.SetTextColor(RGB(255, 255, 255 ));
	m_sg.SetBkColor(RGB( 0, 50, 140 ));
	m_sg.SetTextColor(RGB(255, 255, 255 ));
	/*CFont l_font;
	l_font.CreateFont(14, 0, 0, 0, FW_NORMAL,
	FALSE, FALSE, FALSE, 0, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,   DEFAULT_QUALITY, DEFAULT_PITCH | FF_ROMAN, "Tahoma");
	m_freq1.SetFont(&l_font,TRUE);*/
	//
    // StillCap-specific initialization
    //
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    
    // Default to capturing stills
    //
    CheckDlgButton( IDC_CAPSTILLS, 1 );
    m_bCapStills = true;
    m_nCapState = 0;
    m_nCapTimes = 0;
    g_hwnd = GetSafeHwnd();

    // start up the still image capture graph
    //
    HRESULT hr = InitStillGraph( );
    // Don't display an error message box on failure, because InitStillGraph()
    // will already have displayed a specific error message
  
    // Modify the window style of the capture and still windows
    // to prevent excessive repainting
    m_PreviewScreen.ModifyStyle(0, WS_CLIPCHILDREN);
    return TRUE;  // return TRUE  unless you set the focus to a control
}


void CStillCapDlg::ClearGraphs( )
{
    // Destroy capture graph
    if( m_pGraph )
    {
        // have to wait for the graphs to stop first
        //
        CComQIPtr< IMediaControl, &IID_IMediaControl > pControl = m_pGraph;
        if( pControl ) 
            pControl->Stop( );

        // make the window go away before we release graph
        // or we'll leak memory/resources
        // 
        CComQIPtr< IVideoWindow, &IID_IVideoWindow > pWindow = m_pGraph;
        if( pWindow )
        {
            pWindow->put_Visible( OAFALSE );
            pWindow->put_Owner( NULL );
        }

#ifdef REGISTER_FILTERGRAPH
        // Remove filter graph from the running object table   
        if (g_dwGraphRegister)
            RemoveGraphFromRot(g_dwGraphRegister);
#endif

        m_pGraph.Release( );
        m_pGrabber.Release( );
    }

    // Destroy playback graph, if it exists
    if( m_pPlayGraph )
    {
        CComQIPtr< IMediaControl, &IID_IMediaControl > pControl = m_pPlayGraph;
        if( pControl ) 
            pControl->Stop( );

        CComQIPtr< IVideoWindow, &IID_IVideoWindow > pWindow = m_pPlayGraph;
        if( pWindow )
        {
            pWindow->put_Visible( OAFALSE );
            pWindow->put_Owner( NULL );
        }

        m_pPlayGraph.Release( );
    }
}
HRESULT FindCrossbarPin(
    IAMCrossbar *pXBar,                 // Pointer to the crossbar.
    PhysicalConnectorType PhysicalType, // Pin type to match.
    PIN_DIRECTION Dir,                  // Pin direction.
    long *pIndex)       // Receives the index of the pin, if found.
{
    BOOL bInput = (Dir == PINDIR_INPUT ? TRUE : FALSE);

    // Find out how many pins the crossbar has.
    long cOut, cIn;
    HRESULT hr = pXBar->get_PinCounts(&cOut, &cIn);
    if (FAILED(hr)) return hr;
    // Enumerate pins and look for a matching pin.
    long count = (bInput ? cIn : cOut);
    for (long i = 0; i < count; i++)
    {
        long iRelated = 0;
        long ThisPhysicalType = 0;
        hr = pXBar->get_CrossbarPinInfo(bInput, i, &iRelated,
            &ThisPhysicalType);
        if (SUCCEEDED(hr) && ThisPhysicalType == PhysicalType)
        {
            // Found a match, return the index.
            *pIndex = i;
            return S_OK;
        }
    }
    // Did not find a matching pin.
    return E_FAIL;
}

    /*PhysConn_Video_Tuner,
    PhysConn_Video_Composite,
    PhysConn_Video_SVideo,
    PhysConn_Video_RGB,
    PhysConn_Video_YRYBY,
    PhysConn_Video_SerialDigital,
    PhysConn_Video_ParallelDigital,
    PhysConn_Video_SCSI,
    PhysConn_Video_AUX,
    PhysConn_Video_1394,
    PhysConn_Video_USB,
    PhysConn_Video_VideoDecoder,
    PhysConn_Video_VideoEncoder,
    PhysConn_Video_SCART,
    PhysConn_Video_Black,
    PhysConn_Audio_Tuner,
    PhysConn_Audio_Line,
    PhysConn_Audio_Mic,
    PhysConn_Audio_AESDigital,
    PhysConn_Audio_SPDIFDigital,
    PhysConn_Audio_SCSI,
    PhysConn_Audio_AUX,
    PhysConn_Audio_1394,
    PhysConn_Audio_USB,
    PhysConn_Audio_AudioDecoder*/


HRESULT ConnectTvAudio(IAMCrossbar *pXBar, BOOL bActivate)
{
    // Look for the Audio Decoder output pin.
    long i = 0;
    HRESULT hr = FindCrossbarPin(pXBar, PhysConn_Audio_AudioDecoder,
        PINDIR_OUTPUT, &i);
    if (SUCCEEDED(hr))
    {
        if (bActivate)  // Activate the audio. 
        {
            // Look for the Audio Tuner input pin.
            long j = 0;
            hr = FindCrossbarPin(pXBar, PhysConn_Audio_Tuner, 
			PINDIR_INPUT, &j);
            if (SUCCEEDED(hr))
            {
                return pXBar->Route(i, j);
            }
        }
        else  // Mute the audio
        {
            return pXBar->Route(i, -1);
        }
    }
    return E_FAIL;
}
HRESULT ConnectRadioAudio(IAMCrossbar *pXBar, BOOL bActivate)
{
    // Look for the Audio Decoder output pin.
    long i = 0;
    HRESULT hr = FindCrossbarPin(pXBar, PhysConn_Audio_AudioDecoder,
        PINDIR_OUTPUT, &i);
    if (SUCCEEDED(hr))
    {
        return pXBar->Route(i, 5);
    }
    return E_FAIL;
}

HRESULT ActivateTvAudio(ICaptureGraphBuilder2 *pBuild, IBaseFilter *pSrc,
  BOOL bActivate)
{
    // Search upstream for a crossbar.
    IAMCrossbar *pXBar1 = NULL;
    HRESULT hr = pBuild->FindInterface(&LOOK_UPSTREAM_ONLY, NULL, pSrc,
        IID_IAMCrossbar, (void**)&pXBar1);
    if (SUCCEEDED(hr)) 
    {
        hr = ConnectTvAudio(pXBar1, bActivate);
        if (FAILED(hr))
        {
            // Look for another crossbar.
            IBaseFilter *pF = NULL;
            hr = pXBar1->QueryInterface(IID_IBaseFilter, (void**)&pF);
            if (SUCCEEDED(hr)) 
            {
                // Search upstream for another one.
                IAMCrossbar *pXBar2 = NULL;
                hr = pBuild->FindInterface(&LOOK_UPSTREAM_ONLY, NULL, pF,
                    IID_IAMCrossbar, (void**)&pXBar2);
                pF->Release();
                if (SUCCEEDED(hr))
                {
                    hr = ConnectTvAudio(pXBar2, bActivate);
                    pXBar2->Release();
                }
            }
        }
        pXBar1->Release();
    }
    return hr;
}
HRESULT ActivateRadioAudio(ICaptureGraphBuilder2 *pBuild, IBaseFilter *pSrc,
  BOOL bActivate)
{
    // Search upstream for a crossbar.
    IAMCrossbar *pXBar1 = NULL;
    HRESULT hr = pBuild->FindInterface(&LOOK_UPSTREAM_ONLY, NULL, pSrc,
        IID_IAMCrossbar, (void**)&pXBar1);
    if (SUCCEEDED(hr)) 
    {
        hr = ConnectRadioAudio(pXBar1, bActivate);
        if (FAILED(hr))
        {
            // Look for another crossbar.
            IBaseFilter *pF = NULL;
            hr = pXBar1->QueryInterface(IID_IBaseFilter, (void**)&pF);
            if (SUCCEEDED(hr)) 
            {
                // Search upstream for another one.
                IAMCrossbar *pXBar2 = NULL;
                hr = pBuild->FindInterface(&LOOK_UPSTREAM_ONLY, NULL, pF,
                    IID_IAMCrossbar, (void**)&pXBar2);
                pF->Release();
                if (SUCCEEDED(hr))
                {
                    hr = ConnectRadioAudio(pXBar2, bActivate);
                    pXBar2->Release();
                }
            }
        }
        pXBar1->Release();
    }
    return hr;
}


HRESULT CStillCapDlg::InitStillGraph( )
{
    HRESULT hr;

    // create a filter graph
    //
    hr = m_pGraph.CoCreateInstance( CLSID_FilterGraph );
    if( !m_pGraph )
    {
        Error( TEXT("Could not create filter graph") );
        return E_FAIL;
    }

    // get whatever capture device exists
    //
    
    GetDefaultCapDevice( &pCap );
    if( !pCap )
    {
        Error( TEXT("No video capture device was detected on your system.\r\n\r\n")
               TEXT("This sample requires a functional video capture device, such\r\n")
               TEXT("as a USB web camera.  Video capture will be disabled.") );
        return E_FAIL;
    }

    // add the capture filter to the graph
    //
    hr = m_pGraph->AddFilter( pCap, L"Cap" );
    if( FAILED( hr ) )
    {
        Error( TEXT("Could not put capture device in graph"));
        return E_FAIL;
    }

    // create a sample grabber
    //
    hr = m_pGrabber.CoCreateInstance( CLSID_SampleGrabber );
    if( !m_pGrabber )
    {
        Error( TEXT("Could not create SampleGrabber (is qedit.dll registered?)"));
        return hr;
    }
    CComQIPtr< IBaseFilter, &IID_IBaseFilter > pGrabBase( m_pGrabber );

    // force it to connect to video, 24 bit
    //
    CMediaType VideoType;
    VideoType.SetType( &MEDIATYPE_Video );
    VideoType.SetSubtype( &MEDIASUBTYPE_RGB24 );
    hr = m_pGrabber->SetMediaType( &VideoType ); // shouldn't fail
    if( FAILED( hr ) )
    {
        Error( TEXT("Could not set media type"));
        return hr;
    }

    // add the grabber to the graph
    //
    hr = m_pGraph->AddFilter( pGrabBase, L"Grabber" );
    if( FAILED( hr ) )
    {
        Error( TEXT("Could not put sample grabber in graph"));
        return hr;
    }

    // build the graph
   
    hr = pCGB2.CoCreateInstance (CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC);
    if (FAILED( hr ))
    {
        Error(TEXT ("Can't get a ICaptureGraphBuilder2 reference"));
        return hr;
    }

    hr = pCGB2->SetFiltergraph( m_pGraph );
    if (FAILED( hr ))
    {
        Error(TEXT("SetGraph failed"));
        return hr;
    }


    // If there is a VP pin present on the video device, then put the 
    // renderer on CLSID_NullRenderer
    CComPtr<IPin> pVPPin;
    hr = pCGB2->FindPin(
                        pCap, 
                        PINDIR_OUTPUT, 
                        &PIN_CATEGORY_VIDEOPORT, 
                        NULL, 
                        FALSE, 
                        0, 
                        &pVPPin);


    // If there is a VP pin, put the renderer on NULL Renderer
    CComPtr<IBaseFilter> pRenderer;
    if (S_OK == hr)
    {   
        hr = pRenderer.CoCreateInstance(CLSID_NullRenderer);    
        if (S_OK != hr)
        {
            Error(TEXT("Unable to make a NULL renderer"));
            return S_OK;
        }
        hr = m_pGraph->AddFilter(pRenderer, L"NULL renderer");
        if (FAILED (hr))
        {
            Error(TEXT("Can't add the filter to graph"));
            return hr;
        }
    }

       // try to render preview pin
        hr = pCGB2->RenderStream( 
                                &PIN_CATEGORY_PREVIEW, 
                                &MEDIATYPE_Video,
                                pCap, 
                                pGrabBase, 
                                pRenderer);

			// try to render capture pin
			if( FAILED( hr ) )
			{
				hr = pCGB2->RenderStream( 
										&PIN_CATEGORY_CAPTURE, 
										&MEDIATYPE_Video,
										pCap, 
										pGrabBase, 
										pRenderer);
					
			}
    			// try to render audio pin
					hr = pCGB2->RenderStream( 
													&PIN_CATEGORY_PREVIEW, 
													&MEDIATYPE_Audio,
													pCap, 
													NULL, 
													pRenderer);
					

    if( FAILED( hr ) )
    {
        Error( TEXT("Can't build the graph") );
        return hr;
    }

    // ask for the connection media type so we know how big
    // it is, so we can write out bitmaps
    //
    AM_MEDIA_TYPE mt;
    hr = m_pGrabber->GetConnectedMediaType( &mt );
    if ( FAILED( hr) )
    {
        Error( TEXT("Could not read the connected media type"));
        return hr;
    }
    
    VIDEOINFOHEADER * vih = (VIDEOINFOHEADER*) mt.pbFormat;
    mCB.pOwner = this;
    mCB.lWidth  = vih->bmiHeader.biWidth;
    mCB.lHeight = vih->bmiHeader.biHeight;
    FreeMediaType( mt );

    // don't buffer the samples as they pass through
    //
    hr = m_pGrabber->SetBufferSamples( FALSE );

    // only grab one at a time, stop stream after
    // grabbing one sample
    //
    hr = m_pGrabber->SetOneShot( FALSE );

    // set the callback, so we can grab the one sample
    //
    hr = m_pGrabber->SetCallback( &mCB, 1 );

    // find the video window and stuff it in our window
    //
    CComQIPtr< IVideoWindow, &IID_IVideoWindow > pWindow = m_pGraph;
    if( !pWindow )
    {
        Error( TEXT("Could not get video window interface"));
        return E_FAIL;
    }

    // set up the preview window to be in our dialog
    // instead of floating popup
    //
    HWND hwndPreview = NULL;
    GetDlgItem( IDC_PREVIEW, &hwndPreview );
    RECT rc;
    ::GetWindowRect( hwndPreview, &rc );

    hr = pWindow->put_Owner( (OAHWND) hwndPreview );
    hr = pWindow->put_Left( 0 );
    hr = pWindow->put_Top( 0 );
    hr = pWindow->put_Width( rc.right - rc.left );
    hr = pWindow->put_Height( rc.bottom - rc.top );
    hr = pWindow->put_WindowStyle( WS_CHILD | WS_CLIPSIBLINGS );
    hr = pWindow->put_Visible( OATRUE );
    
    // Add our graph to the running object table, which will allow
    // the GraphEdit application to "spy" on our graph
#ifdef REGISTER_FILTERGRAPH
    hr = AddGraphToRot(m_pGraph, &g_dwGraphRegister);
    if (FAILED(hr))
    {
        Error(TEXT("Failed to register filter graph with ROT!"));
        g_dwGraphRegister = 0;
    }
#endif
	  

    // run the graph
    //
    CComQIPtr< IMediaControl, &IID_IMediaControl > pControl = m_pGraph;
    hr = pControl->Run( );
    if( FAILED( hr ) )
    {
        Error( TEXT("Could not run graph"));
        return hr;
    }
    ///tv tuner
	hr = pCGB2->FindInterface(&PIN_CATEGORY_CAPTURE,
							&MEDIATYPE_Video, pCap,
							IID_IAMTVTuner, (void **)&pTV);
					pTV->Release();
	hr = pTV->put_InputType( 0, TunerInputAntenna );
	AMTunerModeType lmode;
	hr=pTV->get_Mode(&lmode);
	if(lmode==AMTUNER_MODE_TV){
	//route the crossbar pins
	hr = ActivateTvAudio(pCGB2, pCap, TRUE);
	UpdateStatus(_T("Live Video"));
	}else{
	hr = ActivateRadioAudio(pCGB2, pCap, TRUE);
	UpdateStatus(_T("Live Radio"));
	}
    return 0;
}

void CStillCapDlg::GetDefaultCapDevice( IBaseFilter ** ppCap )
{
    HRESULT hr;

    ASSERT(ppCap);
    if (!ppCap)
        return;

    *ppCap = NULL;

    // create an enumerator
    //
    CComPtr< ICreateDevEnum > pCreateDevEnum;
    pCreateDevEnum.CoCreateInstance( CLSID_SystemDeviceEnum );

    ASSERT(pCreateDevEnum);
    if( !pCreateDevEnum )
        return;

    // enumerate video capture devices
    //
    CComPtr< IEnumMoniker > pEm;
    pCreateDevEnum->CreateClassEnumerator( CLSID_VideoInputDeviceCategory, &pEm, 0 );

    ASSERT(pEm);
    if( !pEm )
        return;

    pEm->Reset( );
 
    // go through and find first video capture device
    //
    while( 1 )
    {
        ULONG ulFetched = 0;
        CComPtr< IMoniker > pM;

        hr = pEm->Next( 1, &pM, &ulFetched );
        if( hr != S_OK )
            break;

        // get the property bag interface from the moniker
        //
        CComPtr< IPropertyBag > pBag;
        hr = pM->BindToStorage( 0, 0, IID_IPropertyBag, (void**) &pBag );
        if( hr != S_OK )
            continue;

        // ask for the english-readable name
        //
        CComVariant var;
        var.vt = VT_BSTR;
        hr = pBag->Read( L"FriendlyName", &var, NULL );
        if( hr != S_OK )
            continue;

        // set it in our UI
        //
        USES_CONVERSION;
        SetDlgItemText( IDC_CAPOBJ, W2T( var.bstrVal ) );

        // ask for the actual filter
        //
        hr = pM->BindToObject( 0, 0, IID_IBaseFilter, (void**) ppCap );
        if( *ppCap )
            break;
    }

    return;
}



BOOL CStillCapDlg::DestroyWindow() 
{
    KillTimer(1);
	ClearGraphs( );

    return CDialog::DestroyWindow();
}


void CStillCapDlg::Error( TCHAR * pText )
{
    GetDlgItem( IDC_SNAP )->EnableWindow( FALSE );
    ::MessageBox( NULL, pText, TEXT("Error!"), MB_OK | MB_TASKMODAL | MB_SETFOREGROUND );
}

void CStillCapDlg::UpdateStatus(TCHAR *szStatus)
{
    m_StrStatus.SetWindowText(szStatus);
}


LRESULT CStillCapDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
     
    
    return CDialog::WindowProc(message, wParam, lParam);
}


void CStillCapDlg::OnClose() 
{
    // Free the memory allocated for our bitmap data buffer
    if (cb.pBuffer != 0)
    {
        delete cb.pBuffer;
        cb.pBuffer = 0;
    }
    KillTimer(1); 
    CDialog::OnClose();
}

void CStillCapDlg::OnBnClickedButton1()
{
	 //::MessageBox( NULL, "ggg", TEXT("Error!"), MB_OK | MB_TASKMODAL | MB_SETFOREGROUND );    
	  //HRESULT hr= SetFrequency( 10050000 );
	 ///////////////////////////
     HRESULT hr = pTV->QueryInterface(IID_ISpecifyPropertyPages,
						(void **)&pSpec);
					if(hr == S_OK)
					{
						hr = pSpec->GetPages(&cauuid);
						hr = OleCreatePropertyFrame(ghwndApp, 30, 30, NULL, 1,
							(IUnknown **)&pTV, cauuid.cElems,
							(GUID *)cauuid.pElems, 0, 0, NULL);
						CoTaskMemFree(cauuid.pElems);
						pSpec->Release();
					}       
}

void CStillCapDlg::OnBnClickedButton2()
{
        	IAMCrossbar *pX;
               HRESULT hr = pCGB2->FindInterface(&PIN_CATEGORY_CAPTURE,
                    &MEDIATYPE_Interleaved, pCap,
                    IID_IAMCrossbar, (void **)&pX);
                if(hr != NOERROR)
                    hr = pCGB2->FindInterface(&PIN_CATEGORY_CAPTURE,
                        &MEDIATYPE_Video,pCap,
                        IID_IAMCrossbar, (void **)&pX);

                ISpecifyPropertyPages *pSpec;
                CAUUID cauuid;
                hr = pX->QueryInterface(IID_ISpecifyPropertyPages,
                    (void **)&pSpec);
                if(hr == S_OK)
                {
                    hr = pSpec->GetPages(&cauuid);

                    hr = OleCreatePropertyFrame(ghwndApp, 30, 30, NULL, 1,
                        (IUnknown **)&pX, cauuid.cElems,
                        (GUID *)cauuid.pElems, 0, 0, NULL);

                    CoTaskMemFree(cauuid.pElems);
                    pSpec->Release();
                }
                pX->Release();
}

void CStillCapDlg::OnBnClickedButton3()
{
	HRESULT hr = pTV->put_Mode(AMTUNER_MODE_FM_RADIO);
	hr = ActivateRadioAudio(pCGB2, pCap, TRUE);
	UpdateStatus(_T("Live Radio"));
}
void CStillCapDlg::OnBnClickedButton6()
{
	HRESULT hr = pTV->put_Mode(AMTUNER_MODE_TV);
	hr = ActivateTvAudio(pCGB2, pCap, TRUE);
	UpdateStatus(_T("Live Video"));
}

void CStillCapDlg::OnBnClickedButton4()
{   
	z=1;
	SetTimer(1,0, NULL);	
}
void CStillCapDlg::OnBnClickedButton7()
{
    z=0;
	SetTimer(1,0, NULL);	
}
void CStillCapDlg::OnBnClickedButton8()
{
	KillTimer(1);
}

void CStillCapDlg::OnTimer(UINT nIDEvent) 
{   long FREQ;
	HRESULT hr=pTV->get_VideoFrequency(&FREQ);
	//SetDlgItemInt(IDC_EDIT1,FREQ/1000000,TRUE);
	sprintf( buffer , "%f %s",(float)FREQ/1000000,"Mhz"); 
	m_freq1.SetWindowText(buffer);
if(smode==0)
{
    AMTunerModeType lmode;
	hr=pTV->get_Mode(&lmode);
	if(lmode==AMTUNER_MODE_TV){
		if(l>0 && l<70)
		{    //Sleep(200);
				long plFoundSignal = 0;
				pTV->AutoTune(l, &plFoundSignal);	
				sprintf( buffer , "%0.2f",(float)FREQ/1000000); 
				m_freq1.SetWindowText(buffer);
				sprintf( buffer , "%d",plFoundSignal); 
				m_sg.SetWindowText(buffer);
				sprintf( buffer , "%d",FREQ); 
				m_freq2.SetWindowText(buffer);
				if (plFoundSignal==1)
				{
				KillTimer(1);
				}
				if(z==1)
				{	
					if(l==1){
						l=70;}
					l--;	    
				}
				else{
					if(l==69){
						l=0;}
					l++;
			}
		}
		}else{
			if(k>86500000 && k<108100000)
			{    //Sleep(200);
				long plFoundSignal = 0;
				CString str;
				pTV->AutoTune(k, &plFoundSignal);	
				sprintf( buffer , "%0.2f",(float)FREQ/1000000); 
				m_freq1.SetWindowText(buffer);
				sprintf( buffer , "%d",plFoundSignal); 
				m_sg.SetWindowText(buffer);
				sprintf( buffer , "%d",FREQ); 
				m_freq2.SetWindowText(buffer);
				m_step.GetWindowText(str);
				long step=(long)atoi(str);
				if(z==1){
						if(k==86600000){
							k=108000000;}
						if(k-2*step>86500000)
						{
							k=k-step;
						}else KillTimer(1);

						}
				else{
						if(k==108100000){
							k=86700000;}	
						if(k+2*step<108100000)
						{
							k=k+step;
						}else KillTimer(1);
						}	}
		}
}else if(smode==1)
	{
	    CString str;
		m_freq2.GetWindowText(str);
		long value1=(long)atoi( str );
		m_step.GetWindowText(str);
        long step=(long)atoi( str );
		if(value1>min_frq && value1<max_frq){
		Sleep(50);
		SetFrequency(value1+step);
		if(value1+2*step<max_frq){
		sprintf( buffer , "%d",value1+step); 
		m_freq2.SetWindowText(buffer);}
		else{KillTimer(1);}
		}
}else if(smode==2)
	{
	    CString str;
		m_freq2.GetWindowText(str);
		long value1=(long)atoi( str );
		m_step.GetWindowText(str);
        long step=(long)atoi( str );
		if(value1>min_frq && value1<max_frq){
		Sleep(50);
		SetFrequency(value1-step);
		if(value1-2*step>min_frq){
		sprintf( buffer ,"%d",value1-step); 
		m_freq2.SetWindowText(buffer);}
		else{KillTimer(1);}
		}
	}
	CDialog::OnTimer(nIDEvent);
}
#define INSTANCEDATA_OF_PROPERTY_PTR(x) ((PKSPROPERTY((x))) + 1)
#define INSTANCEDATA_OF_PROPERTY_SIZE(x) (sizeof((x)) - sizeof(KSPROPERTY))
void CStillCapDlg::SetFrequency(long Freq)
{ 
    HRESULT hr;
    DWORD dwSupported=0;  
    DWORD cbBytes=0;
    // Query the IKsPropertySet on your Device TV Tuner Filter.
    // m_pTvtuner is IBaseFilter Pointer of your TV Tuner Filter.   
    CComPtr<IKsPropertySet> m_pKSProp;
    hr = pTV->QueryInterface(IID_IKsPropertySet, (void**)&m_pKSProp); 
    if (FAILED(hr))
		::MessageBox( NULL, "QueryInterface Not Supported", TEXT("Error!"), MB_OK | MB_TASKMODAL | MB_SETFOREGROUND ); 
	/////////////////////get tuner mode////////
    //KSPROPERTY_TUNER_MODE_S TunerMode;
	//memset(&TunerMode,0,sizeof(KSPROPERTY_TUNER_MODE_S));
	//hr=m_pKSProp->Get(PROPSETID_TUNER,
	//		KSPROPERTY_TUNER_MODE,
	//		INSTANCEDATA_OF_PROPERTY_PTR(&TunerMode),
	//		INSTANCEDATA_OF_PROPERTY_SIZE(TunerMode),
	//		&TunerMode,
	//		sizeof(TunerMode),
	//		&cbBytes);
	////////////////////////////////////////
	KSPROPERTY_TUNER_MODE_CAPS_S ModeCaps;
    KSPROPERTY_TUNER_FREQUENCY_S Frequency;
    memset(&ModeCaps,0,sizeof(KSPROPERTY_TUNER_MODE_CAPS_S));
    memset(&Frequency,0,sizeof(KSPROPERTY_TUNER_FREQUENCY_S));
    //ModeCaps.Mode = TunerMode.Mode; 
	//ModeCaps.Mode = AMTUNER_MODE_FM_RADIO;
	ModeCaps.Mode = AMTUNER_MODE_TV; 
    // Check either the Property is supported or not by the Tuner drivers 
    hr = m_pKSProp->QuerySupported(PROPSETID_TUNER, 
          KSPROPERTY_TUNER_MODE_CAPS,&dwSupported);
    if(SUCCEEDED(hr) && dwSupported&KSPROPERTY_SUPPORT_GET)
    {
        DWORD cbBytes=0;
        hr = m_pKSProp->Get(PROPSETID_TUNER,KSPROPERTY_TUNER_MODE_CAPS,
            INSTANCEDATA_OF_PROPERTY_PTR(&ModeCaps),
            INSTANCEDATA_OF_PROPERTY_SIZE(ModeCaps),
            &ModeCaps,
            sizeof(ModeCaps),
            &cbBytes);  
	}
    else
		::MessageBox( NULL, "QuerySupported Not Supported", TEXT("Error!"), MB_OK | MB_TASKMODAL | MB_SETFOREGROUND ); 
    Frequency.Frequency=Freq;
    if(ModeCaps.Strategy==KS_TUNER_STRATEGY_DRIVER_TUNES)
        Frequency.TuningFlags=KS_TUNER_TUNING_FINE;
    else
        Frequency.TuningFlags=KS_TUNER_TUNING_EXACT;
    // Here the real magic starts
	if(Freq>=ModeCaps.MinFrequency && Freq<=ModeCaps.MaxFrequency)
    {
        hr = m_pKSProp->Set(PROPSETID_TUNER,
            KSPROPERTY_TUNER_FREQUENCY,
            INSTANCEDATA_OF_PROPERTY_PTR(&Frequency),
            INSTANCEDATA_OF_PROPERTY_SIZE(Frequency),
            &Frequency,
            sizeof(Frequency));
		/*if(FAILED(hr))
			::MessageBox( NULL, "Frequency Not Set", TEXT("Error!"), MB_OK | MB_TASKMODAL | MB_SETFOREGROUND ); */
    }
    else
		::MessageBox( NULL, "Frequency range passed over!", TEXT("Error!"), MB_OK | MB_TASKMODAL | MB_SETFOREGROUND ); 
	////////////////get frequency///////////////////////////    
	  	KSPROPERTY_TUNER_STATUS_S TunerStatus;
		hr=m_pKSProp->Get(PROPSETID_TUNER,
									KSPROPERTY_TUNER_STATUS,
									INSTANCEDATA_OF_PROPERTY_PTR(&TunerStatus),
									INSTANCEDATA_OF_PROPERTY_SIZE(TunerStatus),
									&TunerStatus,
									sizeof(TunerStatus),
									&cbBytes);  
		sprintf( buffer , "%0.2f",(float)TunerStatus.CurrentFrequency/1000000); 
	    m_freq1.SetWindowText(buffer);	
}
void CStillCapDlg::OnBnClickedButton5()
{   	
	CString str;
	m_freq2.GetWindowText(str);
    long value1=(long)atoi( str );
	if(value1>min_frq && value1<max_frq)
	{
		SetFrequency(value1);
	}	
}

void CStillCapDlg::OnBnClickedButton10()
{
	CString str;
	m_freq2.GetWindowText(str);
    long value1=(long)atoi( str );
	m_step.GetWindowText(str);
    long step=(long)atoi( str );
	if(value1>min_frq && value1<max_frq){
	SetFrequency(value1+step);
	if(value1+step<max_frq){
		sprintf( buffer , "%d",value1+step);
	    m_freq2.SetWindowText(buffer);	}}
	if(smode!=0)
	{smode=1;}
}

void CStillCapDlg::OnBnClickedButton11()
{
	CString str;
	m_freq2.GetWindowText(str);
    long value1=(long)atoi( str );
	m_step.GetWindowText(str);
    long step=(long)atoi( str );
	if(value1>min_frq && value1<max_frq){
	SetFrequency(value1-step);
	if(value1-step>min_frq){
		sprintf( buffer , "%d",value1-step);
	    m_freq2.SetWindowText(buffer);}}
	if(smode!=0)
	{smode=2;}

}

void CStillCapDlg::OnBnClickedButton9()
{
	if(smode==0){
	smode=1;
	SetTimer(1,0, NULL);
	m_button9.SetWindowText ("Stop");
	}
	else{
	smode=0;
	KillTimer(1);
	m_button9.SetWindowText ("Auto_Scan");
	}
}
