//------------------------------------------------------------------------------
// File: StillCapDlg.h
//
// Desc: DirectShow sample code - definition of callback and dialog
//       classes for StillCap application.
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------


#if !defined(AFX_STILLCAPDLG_H__3067E9D2_B94C_4ED1_99AB_53034129A0DD__INCLUDED_)
#define AFX_STILLCAPDLG_H__3067E9D2_B94C_4ED1_99AB_53034129A0DD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "ColorCtrl.h"
#include "FontCtrl.h"
/////////////////////////////////////////////////////////////////////////////
// CStillCapDlg dialog

class CSampleGrabberCB;

class CStillCapDlg : public CDialog
{
    friend class CSampleGrabberCB;

protected:

    // either the capture live graph, or the capture still graph
    CComPtr< IGraphBuilder > m_pGraph;

    // the playback graph when capturing video
    CComPtr< IGraphBuilder > m_pPlayGraph;

    // the sample grabber for grabbing stills
    CComPtr< ISampleGrabber > m_pGrabber;

    // if you're in still mode or capturing video mode
    bool m_bCapStills;

    // when in video mode, whether capturing or playing back
    int m_nCapState;

    // how many times you've captured
    int m_nCapTimes;
    HRESULT InitStillGraph( );
    HRESULT InitCaptureGraph( TCHAR * pFilename );
    HRESULT InitPlaybackGraph( TCHAR * pFilename ); 
    void GetDefaultCapDevice( IBaseFilter ** ppCap );
    void ClearGraphs( );
    void UpdateStatus(TCHAR *szStatus);
    void Error( TCHAR * pText );
	void SetFrequency(long Freq);
	
// Construction
public:
    CStillCapDlg(CWnd* pParent = NULL); // standard constructor

// Dialog Data
    //{{AFX_DATA(CStillCapDlg)
     enum { IDD = IDD_STILLCAP_DIALOG };
		CColorCtrl<CBoldCtrl <CStatic> > m_StrStatus;
		CColorCtrl<CBoldCtrl <CStatic> > m_PreviewScreen;
		CColorCtrl<CBoldCtrl <CStatic> > m_mhz;
		CColorCtrl<CBoldCtrl <CStatic> > m_hz;
		CColorCtrl<CBoldCtrl <CStatic> > m_hz2;
		CColorCtrl<CBoldCtrl <CStatic> > m_lblstep;
		CColorCtrl<CBoldCtrl <CStatic> > m_lblsg;
		CStatic m_ST1;
		CStatic m_ST2;
		CColorCtrl<CEdit> m_freq1;
		CColorCtrl<CEdit> m_freq2;
		CColorCtrl<CEdit> m_step;
		CColorCtrl<CEdit> m_sg;
		CColorPushButton< CColorCtrl<CButton> >  m_button1;
		CColorPushButton< CColorCtrl<CButton> >  m_button2;
		CColorPushButton< CColorCtrl<CButton> >  m_button3;
		CColorPushButton< CColorCtrl<CButton> >  m_button4;
		CColorPushButton< CColorCtrl<CButton> >  m_button5;
		CColorPushButton< CColorCtrl<CButton> >  m_button6;
		CColorPushButton< CColorCtrl<CButton> >  m_button7;
		CColorPushButton< CColorCtrl<CButton> >  m_button8;
		CColorPushButton< CColorCtrl<CButton> >  m_button9;
		CColorPushButton< CColorCtrl<CButton> >  m_button10;
		CColorPushButton< CColorCtrl<CButton> >  m_button11;
		
	//}}AFX_DATA

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CStillCapDlg)
    public:
    virtual BOOL DestroyWindow();
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    //}}AFX_VIRTUAL

// Implementation
protected:
    HICON m_hIcon;

    // Generated message map functions
    //{{AFX_MSG(CStillCapDlg)
    virtual BOOL OnInitDialog();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnClose();
	afx_msg void OnTimer(UINT nIDEvent);
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnBnClickedButton3();
	afx_msg void OnBnClickedButton4();
	afx_msg void OnBnClickedButton5();
	afx_msg void OnBnClickedButton6();
	afx_msg void OnBnClickedButton7();
	afx_msg void OnBnClickedButton8();
	afx_msg void OnBnClickedButton10();
	afx_msg void OnBnClickedButton11();
	afx_msg void OnBnClickedButton9();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STILLCAPDLG_H__3067E9D2_B94C_4ED1_99AB_53034129A0DD__INCLUDED_)
