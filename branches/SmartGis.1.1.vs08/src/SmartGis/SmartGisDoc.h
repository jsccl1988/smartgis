// SmartGisDoc.h : CSmartGisDoc ��Ľӿ�
//


#pragma once


class CSmartGisDoc : public CDocument
{
protected: // �������л�����
	CSmartGisDoc();
	DECLARE_DYNCREATE(CSmartGisDoc)

// ����
public:

// ����
public:

// ��д
public:
	virtual BOOL				OnNewDocument();
	virtual void				Serialize(CArchive& ar);
	virtual	HMENU				GetDefaultMenu() ;

// ʵ��
public:
	virtual ~CSmartGisDoc();
#ifdef _DEBUG
	virtual void				AssertValid() const;
	virtual void				Dump(CDumpContext& dc) const;
#endif

protected:

// ���ɵ���Ϣӳ�亯��
protected:
	DECLARE_MESSAGE_MAP()
public:
	HMENU						m_hCurMainMenu;
};


