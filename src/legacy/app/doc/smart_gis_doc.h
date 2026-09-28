// Session / map open logic lives in SmtApp (core/) and endgame
// content::MapContents / app::MapScene — not in this document class.
// TODO(sp3): Keep as a thin CDocument for MDI templates; do not grow GIS state
// here.

#pragma once

class CSmartGisDoc : public CDocument {
 protected:
  CSmartGisDoc();
  DECLARE_DYNCREATE(CSmartGisDoc)

 public:
  virtual BOOL OnNewDocument();
  virtual void Serialize(CArchive& ar);
  virtual HMENU GetDefaultMenu();

 public:
  virtual ~CSmartGisDoc();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

 protected:
  DECLARE_MESSAGE_MAP()
 public:
  HMENU m_hCurMainMenu;
};
