#include "ado_adorecordset.h"
#include "smt_logmanager.h"

using namespace Smt_Core;
#include <math.h>
#include <io.h>

namespace Smt_Ado
{
	struct ComIniter
	{
		bool bInit;
		ComIniter():bInit(false)
		{
			if( FAILED(::CoInitialize(NULL)) ) 
			{
				bInit = false;
			} 

			bInit = true;
		}

		~ComIniter()
		{
			::CoUninitialize();
		}
	};

	static ComIniter comIniter;

	SmtAdoRecordSet::SmtAdoRecordSet()
	{
		m_pConnection = NULL;
		m_SearchDirection = adSearchForward;
		m_pRecordset.CreateInstance("ADODB.Recordset");
#ifdef _DEBUG
		if (m_pRecordset == NULL)
		{
		//	AfxMessageBox("RecordSet 对象创建失败! 请确认是否初始化了COM环境.");
		}
#endif
		assert(m_pRecordset != NULL);

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(C_STR_ADO_LOG);
		if (NULL == pLog)
		{
			pLogMgr->CreateLog(C_STR_ADO_LOG.c_str());
		}
	}

	SmtAdoRecordSet::SmtAdoRecordSet(SmtAdoConnection *pConnection)
	{
		m_SearchDirection = adSearchForward;
		m_pConnection = pConnection;
		assert(m_pConnection != NULL);
		m_pRecordset.CreateInstance("ADODB.Recordset");
#ifdef _DEBUG
		if (m_pRecordset == NULL)
		{
		//	AfxMessageBox("RecordSet 对象创建失败! 请确认是否初始化了COM环境.");
		}
#endif
		assert(m_pRecordset != NULL);
	}

	SmtAdoRecordSet::~SmtAdoRecordSet()
	{
		Release();
	}

	/*========================================================================
	Params:		
	- strSQL:		SQL语句, 表名, 存储过程调用或持久 Recordset 文件名.
	- CursorType:   可选. CursorTypeEnum 值, 确定打开 Recordset 时应该
	使用的游标类型. 可为下列常量之一.
	[常量]				[说明] 
	-----------------------------------------------
	adOpenForwardOnly	打开仅向前类型游标. 
	adOpenKeyset		打开键集类型游标. 
	adOpenDynamic		打开动态类型游标. 
	adOpenStatic		打开静态类型游标. 
	-----------------------------------------------
	- LockType:     可选, 确定打开 Recordset 时应该使用的锁定类型(并发)
	的 LockTypeEnum 值, 可为下列常量之一.
	[常量]				[说明] 
	-----------------------------------------------
	adLockReadOnly		只读 - 不能改变数据. 
	adLockPessimistic	保守式锁定 - 通常通过在编辑时立即锁定数据源的记录. 
	adLockOptimistic	开放式锁定 - 只在调用 Update 方法时才锁定记录. 
	adLockBatchOptimistic 开放式批更新 - 用于批更新模式(与立即更新模式
	相对). 
	-----------------------------------------------
	- lOption		可选. 长整型值, 用于指示 strSQL 参数的类型. 可为下
	列常量之一.
	[常量]				[说明] 
	-------------------------------------------------
	adCmdText			指示strSQL为命令文本, 即普通的SQL语句. 
	adCmdTable			指示ADO生成SQL查询返回以 strSQL 命名的表中的
	所有行. 
	adCmdTableDirect	指示所作的更改在strSQL中命名的表中返回所有行. 
	adCmdStoredProc		指示strSQL为存储过程. 
	adCmdUnknown		指示strSQL参数中的命令类型为未知. 
	adCmdFile			指示应从在strSQL中命名的文件中恢复保留(保存的)
	Recordset. 
	adAsyncExecute		指示应异步执行strSQL. 
	adAsyncFetch		指示在提取 Initial Fetch Size 属性中指定的初始
	数量后, 应该异步提取所有剩余的行. 如果所需的行尚未
	提取, 主要的线程将被堵塞直到行重新可用. 
	adAsyncFetchNonBlocking 指示主要线程在提取期间从未堵塞. 如果所请求
	的行尚未提取, 当前行自动移到文件末尾. 
	==========================================================================*/
	BOOL SmtAdoRecordSet::Open(LPCTSTR strSQL, long lOption, CursorTypeEnum CursorType, LockTypeEnum LockType)
	{
		assert(m_pConnection != NULL);
		assert(m_pRecordset != NULL);
	
		if(strcmp(strSQL,"") != 0)
		{
			m_strSQL = strSQL;
		}
		if (m_pConnection == NULL || m_pRecordset == NULL)
		{
			return FALSE;
		}

		if (m_strSQL.length() == 0)
		{
			assert(FALSE);
			return FALSE;
		}

		try
		{
			if (IsOpen()) Close();
			return SUCCEEDED(m_pRecordset->Open(_variant_t(m_strSQL.c_str()), 
				_variant_t((IDispatch*)m_pConnection->GetConnection(), true),
				CursorType, LockType, lOption));
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: 打开记录集发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
		//	TRACE(_T("%s\r\n"), GetLastError());
			return FALSE;
		}
	}

	/*========================================================================
	Name:		通过重新执行对象所基于的查询, 更新 Recordset 对象中的数据.
	----------------------------------------------------------
	Params:		Options   可选. 指示影响该操作选项的位屏蔽. 如果该参数设置
	为 adAsyncExecute, 则该操作将异步执行并在它结束时产生 
	RecordsetChangeComplete 事件
	----------------------------------------------------------
	Remarks:	通过重新发出原始命令并再次检索数据, 可使用 Requery 方法刷新
	来自数据源的 Recordset 对象的全部内容. 调用该方法等于相继调用 Close 和 
	Open 方法. 如果正在编辑当前记录或者添加新记录将产生错误.
	==========================================================================*/
	BOOL SmtAdoRecordSet::Requery(long Options)
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return (m_pRecordset->Requery(Options) == S_OK);
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: Requery 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return	FALSE; 
	}

	/*========================================================================
	Name:		从基本数据库刷新当前 Recordset 对象中的数据.	
	----------------------------------------------------------
	Params:		AffectRecords:   可选, AffectEnum 值, 决定 Resync 方法所影
	响的记录数目, 可以为下列常量之一.
	[常量]				[说明]
	------------------------------------
	adAffectCurrent		只刷新当前记录. 
	adAffectGroup		刷新满足当前 Filter 属性设置的记录.只有将 Filter
	属性设置为有效预定义常量之一才能使用该选项. 
	adAffectAll			默认值.刷新 Recordset 对象中的所有记录, 包括由
	于当前 Filter 属性设置而隐藏的记录. 
	adAffectAllChapters 刷新所有子集记录. 

	ResyncValues:   可选, ResyncEnum 值. 指定是否覆盖基本值. 可为下列
	常量之一.
	[常量]				[说明] 
	------------------------------------
	adResyncAllValues	默认值. 覆盖数据, 取消挂起的更新. 
	adResyncUnderlyingValues 不覆盖数据, 不取消挂起的更新. 
	----------------------------------------------------------
	Remarks:	使用 Resync 方法将当前 Recordset 中的记录与基本的数据库重新
	同步. 这在使用静态或仅向前的游标但希望看到基本数据库中的改动时十分有用.
	如果将 CursorLocation 属性设置为 adUseClient, 则 Resync 仅对非只读的 
	Recordset 对象可用.
	与 Requery 方法不同, Resync 方法不重新执行 Recordset 对象的基本的命令, 
	基本的数据库中的新记录将不可见.
	==========================================================================*/
	BOOL SmtAdoRecordSet::Resync(AffectEnum AffectRecords, ResyncEnum ResyncValues)
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return (m_pRecordset->Resync(AffectRecords, ResyncValues) == S_OK);
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: Resync 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return	FALSE; 
	}

	/*========================================================================
	Name:		将 Recordset 保存在持久性文件中.
	----------------------------------------------------------
	Params:		
	[strFileName]:   可选. 文件的完整路径名, 用于保存 Recordset.
	[PersistFormat]:   可选. PersistFormatEnum 值, 指定保存 Recordset 所使
	用的格式. 可以是如下的某个常量: 
	[常量]			[说明] 
	------------------------------
	adPersistADTG	使用专用的"Advanced Data Tablegram"格式保存. 
	adPersistXML	(默认)使用 XML 格式保存. 
	----------------------------------------------------------
	Remarks:	只能对打开的 Recordset 调用 Save 方法. 随后使用 Load 方法可
	以从文件中恢复 Recordset. 如果 Filter 属性影响 Recordset, 将只保存经过
	筛选的行.
	在第一次保存 Recordset 时指定 FileName. 如果随后调用 Save 时, 应忽
	略 FileName, 否则将产生运行时错误. 如果随后使用新的 FileName 调用 Save, 
	那么 Recordset 将保存到新的文件中, 但新文件和原始文件都是打开的.
	==========================================================================*/
	BOOL SmtAdoRecordSet::Save(LPCTSTR strFileName, PersistFormatEnum PersistFormat)
	{
		assert(m_pRecordset != NULL);
		assert(IsOpen());

		if (m_strFileName == strFileName)
		{
			strFileName = NULL;
		}
		else if(_access(strFileName, 0) != -1)
		{
			DeleteFile(strFileName);
			m_strFileName = strFileName;
		}
		else
		{
			m_strFileName = strFileName;
		}

		try
		{
			if (m_pRecordset != NULL) 
			{
				return (m_pRecordset->Save(_bstr_t(strFileName), PersistFormat) == S_OK);
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: Save 发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	BOOL SmtAdoRecordSet::Load(LPCTSTR strFileName)
	{
		if (IsOpen()) Close();

		try
		{
			return (m_pRecordset->Open(strFileName, "Provider=MSPersist;", adOpenForwardOnly, adLockOptimistic, adCmdFile) == S_OK);
		}
		catch (_com_error &e)
		{
			//TRACE(_T("Warning: Load 发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	/*========================================================================
	Name:		取消执行挂起的异步 Execute 或 Open 方法的调用.
	-----------------------------------------------------
	Remarks:	使用 Cancel 方法终止执行异步 Execute 或 Open 方法调用(即通
	过 adAsyncConnect、adAsyncExecute 或 adAsyncFetch 参数调用的方法).
	如果在试图终止的方法中没有使用 adAsyncExecute, 则 Cancel 将返回运行
	时错误.
	==========================================================================*/
	BOOL SmtAdoRecordSet::Cancel()
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->Cancel();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: Cancel发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Name:		关闭打开的对象及任何相关对象.
	-----------------------------------------------------
	Remarks:	使用 Close 方法可关闭 Recordset 对象以便释放所有关联的系统
	资源. 关闭对象并非将它从内存中删除, 可以更改它的属性设置并且在此后
	再次打开. 要将对象从内存中完全删除, 可将对象变量设置为 NULL.
	如果正在立即更新模式下进行编辑, 调用Close方法将产生错误,应首先
	调用 Update 或 CancelUpdat 方法. 如果在批更新期间关闭 Recordset 对
	象, 则自上次 UpdateBatch 调用以来所做的修改将全部丢失.
	如果使用 Clone 方法创建已打开的 Recordset 对象的副本, 关闭原始
	Recordset或其副本将不影响任何其他副本.
	==========================================================================*/
	void SmtAdoRecordSet::Close()
	{
		try
		{
			if (m_pRecordset != NULL && m_pRecordset->State != adStateClosed)
			{
				if (GetEditMode() == adEditNone) 
					CancelUpdate();

				m_pRecordset->Close();
			}
		}
		catch (const _com_error& e)
		{
		//	TRACE(_T("Warning: 关闭记录集发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
		}
	}

	/*========================================================================
	Name:	关闭连接并释放对象.
	-----------------------------------------------------
	Remarks: 关闭连接并释放SmtAdoRecordSet对象, 这样基本上从内容中完全清除了
	SmtAdoRecordSet对象.
	==========================================================================*/
	void SmtAdoRecordSet::Release()
	{
		if (IsOpen()) 
			Close();
		m_pRecordset.Release();
		m_pRecordset = NULL;
	}


	/*########################################################################
	------------------------------------------------
	记录集更新操作
	------------------------------------------------
	########################################################################*/

	/*========================================================================
	Remarks:	开始添加新的纪录. 
	==========================================================================*/
	BOOL SmtAdoRecordSet::AddNew()
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				if (m_pRecordset->AddNew() == S_OK)
				{
					return TRUE;
				}
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: AddNew 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return	FALSE;
	}

	/*========================================================================
	Remarks:	在调用 AddNew 等方法后, 调用此方法完成更新或修改.
	==========================================================================*/
	BOOL SmtAdoRecordSet::Update()
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				if (m_pRecordset->Update() == S_OK)
				{
					return TRUE;
				}
			}
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: Update 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
		}

		CancelUpdate();
		return	FALSE;
	}

	/*========================================================================
	Name:		将所有挂起的批更新写入磁盘.
	----------------------------------------------------------
	Params:		AffectRecords   可选, AffectEnum 值. 决定 UpdateBatch 方法
	所影响的记录数目.可以为如下常量之一.
	[常量]				[说明] 
	------------------------------------
	adAffectCurrent		只写入当前记录的挂起更改. 
	adAffectGroup		写入满足当前 Filter 属性设置的记录所发生的挂起
	更改. 必须将 Filter 属性设置为某个有效的预定义常量才能使用该选项. 
	adAffectAll			(默认值). 写入 Recordset 对象中所有记录的挂起
	更改, 包括由于当前 Filter 属性设置而隐藏的任何记录. 
	adAffectAllChapters 写入所有子集的挂起更改. 
	----------------------------------------------------------
	Remarks:	按批更新模式修改 Recordset 对象时, 使用 UpdateBatch 方法可
	将 Recordset 对象中的所有更改传递到基本数据库.
	如果 Recordset 对象支持批更新, 那么可以将一个或多个记录的多重更改缓存在
	本地, 然后再调用 UpdateBatch 方法. 如果在调用 UpdateBatch 方法时正在编
	辑当前记录或者添加新的记录, 那么在将批更新传送到提供者之前, ADO 将自动
	调用 Update 方法保存对当前记录的所有挂起更改.
	只能对键集或静态游标使用批更新.
	==========================================================================*/
	BOOL SmtAdoRecordSet::UpdateBatch(AffectEnum AffectRecords)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetDefaultLog();

		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return (m_pRecordset->UpdateBatch(AffectRecords) == S_OK);
			}
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: UpdateBatch 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			pLog->LogMessage("Warning: UpdateBatch 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n", e.ErrorMessage(), __FILE__, __LINE__);

			return FALSE;
		} 
		return	FALSE; 
	}

	/*========================================================================
	Name:		取消在调用 Update 方法前对当前记录或新记录所作的任何更改.
	-----------------------------------------------------
	Remarks:	使用 CancelUpdate 方法可取消对当前记录所作的任何更改或放弃
	新添加的记录. 在调用 Update 方法后将无法撤消对当前记录或新记录所做的更
	改, 除非更改是可以用 RollbackTrans 方法回卷的事务的一部分, 或者是可以
	用 CancelBatch 方法取消的批更新的一部分.
	如果在调用 CancelUpdate 方法时添加新记录, 则调用 AddNew 之前的当前
	记录将再次成为当前记录.
	如果尚未更改当前记录或添加新记录, 调用 CancelUpdate 方法将产生错误.
	==========================================================================*/
	BOOL SmtAdoRecordSet::CancelUpdate()
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				if (GetEditMode() == adEditNone || m_pRecordset->CancelUpdate() == S_OK)
				{
					return TRUE;
				}
			}
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: CancelUpdate 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Name:		取消挂起的批更新.
	-----------------------------------------------------
	Params:		AffectRecords   可选的 AffectEnum 值, 决定CancelBatch 方法
	所影响记录的数目, 可为下列常量之一: 
	[常量]			[说明] 
	-------------------------------------------------
	AdAffectCurrent 仅取消当前记录的挂起更新. 
	AdAffectGroup	对满足当前 Filter 属性设置的记录取消挂起更新.
	使用该选项时,必须将 Filter 属性设置为合法的预
	定义常量之一. 
	AdAffectAll		默认值.取消 Recordset 对象中所有记录的挂起更
	新,包括由当前 Filter 属性设置所隐藏的任何记录. 
	==========================================================================*/
	BOOL SmtAdoRecordSet::CancelBatch(AffectEnum AffectRecords)
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return (m_pRecordset->CancelBatch(AffectRecords) == S_OK);
			}
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: CancelBatch 发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Params:		 AffectRecords:  AffectEnum 值, 确定 Delete 方法所影响的记
	录数目, 该值可以是下列常量之一.
	[常量]				[说明 ]
	-------------------------------------------------
	AdAffectCurrent		默认. 仅删除当前记录. 
	AdAffectGroup		删除满足当前 Filter 属性设置的记录. 要使用该选
	项, 必须将 Filter 属性设置为有效的预定义常量之一. 
	adAffectAll			删除所有记录. 
	adAffectAllChapters 删除所有子集记录. 
	==========================================================================*/
	BOOL SmtAdoRecordSet::Delete(AffectEnum AffectRecords)
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return (m_pRecordset->Delete(AffectRecords) == S_OK);
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: Delete发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return	FALSE;
	}

	/*########################################################################
	------------------------------------------------
	记录集导航操作
	------------------------------------------------
	########################################################################*/

	/*========================================================================
	Name:		将当前记录位置移动到 Recordse 中的第一个记录.
	==========================================================================*/
	BOOL SmtAdoRecordSet::MoveFirst()
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return SUCCEEDED(m_pRecordset->MoveFirst());
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: MoveFirst 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return	FALSE;
	}

	/*========================================================================
	Name:		将当前记录位置移动到 Recordset 中的最后一个记录.
	-----------------------------------------------------
	Remarks:	Recordset 对象必须支持书签或向后光标移动; 否则调用该方法将
	产生错误.
	==========================================================================*/
	BOOL SmtAdoRecordSet::MoveLast()
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return SUCCEEDED(m_pRecordset->MoveLast());
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: MoveLast 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return	FALSE;
	}

	/*========================================================================
	Name:		将当前记录位置向后移动一个记录(向记录集的顶部).
	-----------------------------------------------------
	Remarks:	Recordset 对象必须支持书签或向后游标移动; 否则方法调用将产
	生错误.如果首记录是当前记录并且调用 MovePrevious 方法, 则 ADO 将当前记
	录设置在 Recordset (BOF为True) 的首记录之前. 而BOF属性为 True 时向后移
	动将产生错误. 如果 Recordse 对象不支持书签或向后游标移动, 则 MovePrevious 
	方法将产生错误.
	==========================================================================*/
	BOOL SmtAdoRecordSet::MovePrevious()
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return SUCCEEDED(m_pRecordset->MovePrevious());
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: MovePrevious 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
		return FALSE;	
	}

	/*========================================================================
	Name:		将当前记录向前移动一个记录(向 Recordset 的底部).
	-----------------------------------------------------
	Remarks:	如果最后一个记录是当前记录并且调用 MoveNext 方法, 则 ADO 将
	当前记录设置到 Recordset (EOF为 True)的尾记录之后. 当 EOF 属性已经为 
	True 时试图向前移动将产生错误.
	==========================================================================*/
	BOOL SmtAdoRecordSet::MoveNext()
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return SUCCEEDED(m_pRecordset->MoveNext());
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: MoveNext 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
		return FALSE;
	}

	/*========================================================================
	Name:		移动 Recordset 对象中当前记录的位置.
	----------------------------------------------------------
	Params:		
	- lRecords    带符号长整型表达式, 指定当前记录位置移动的记录数.
	- Start    可选, 字符串或变体型, 用于计算书签. 也可为下列 
	BookmarkEnum 值之一: 
	[常量]				[说明] 
	--------------------------------
	adBookmarkCurrent	默认. 从当前记录开始. 
	adBookmarkFirst		从首记录开始. 
	adBookmarkLast		从尾记录开始. 
	==========================================================================*/
	BOOL SmtAdoRecordSet::Move(long lRecords, _variant_t Start)
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return SUCCEEDED(m_pRecordset->Move(lRecords, _variant_t(Start)));
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: Move 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return	FALSE;
	}

	/*########################################################################
	------------------------------------------------
	记录集属性
	------------------------------------------------
	########################################################################*/

	/*========================================================================
	Name:		取得记录集对象的状态(是打开状态还是关闭状态). 对异步方式执
	行的 Recordset 对象, 则说明当前的对象状态是连接、执行还是获取状态.
	-----------------------------------------------------
	returns:	返回下列常量之一的长整型值.
	[常量]				[说明] 
	----------------------------------
	adStateClosed		指示对象是关闭的. 
	adStateOpen			指示对象是打开的. 
	adStateConnecting	指示 Recordset 对象正在连接. 
	adStateExecuting	指示 Recordset 对象正在执行命令. 
	adStateFetching		指示 Recordset 对象的行正在被读取. 
	-----------------------------------------------------
	Remarks:	 可以随时使用 State 属性确定指定对象的当前状态. 该属性是只
	读的. Recordset 对象的 State 属性可以是组合值. 例如: 如果正在执行语句,
	该属性将是 adStateOpen 和 adStateExecuting 的组合值.
	==========================================================================*/
	long SmtAdoRecordSet::GetState()
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->GetState();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetState 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		} 
		return -1;
	}

	BOOL SmtAdoRecordSet::IsOpen()
	{
		try
		{
			return (m_pRecordset != NULL && (GetState() & adStateOpen));
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: IsOpen方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Name:		指示有关批更新或其他大量操作的当前记录的状态.
	-----------------------------------------------------
	returns:	返回下列一个或多个 RecordStatusEnum 值之和.
	[常量]						[说明]
	-------------------------------------------------
	adRecOK						成功地更新记录. 
	adRecNew					记录是新建的. 
	adRecModified				记录被修改. 
	adRecDeleted				记录被删除. 
	adRecUnmodified				记录没有修改. 
	adRecInvalid				由于书签无效, 记录没有保存. 
	adRecMultipleChanges		由于影响多个记录, 因此记录未被保存. 
	adRecPendingChanges			由于记录引用挂起的插入, 因此未被保存. 
	adRecCanceled				由于操作被取消, 未保存记录. 
	adRecCantRelease			由于现有记录锁定, 没有保存新记录. 
	adRecConcurrencyViolation	由于开放式并发在使用中, 记录未被保存. 
	adRecIntegrityViolation		由于用户违反完整性约束, 记录未被保存. 
	adRecMaxChangesExceeded		由于存在过多挂起更改, 记录未被保存. 
	adRecObjectOpen				由于与打开的储存对象冲突, 记录未被保存. 
	adRecOutOfMemory			由于计算机内存不足, 记录未被保存. 
	adRecPermissionDenied		由于用户没有足够的权限, 记录未被保存. 
	adRecSchemaViolation		由于记录违反基本数据库的结构, 因此未被保存. 
	adRecDBDeleted				记录已经从数据源中删除. 
	-----------------------------------------------------
	Remarks:	使用 Status 属性查看在批更新中被修改的记录有哪些更改被挂起.
	也可使用 Status 属性查看大量操作时失败记录的状态. 例如, 调用 Recordset
	对象的 Resync、UpdateBatch 或 CancelBatch 方法, 或者设置 Recordset 对象
	的 Filter 属性为书签数组. 使用该属性, 可检查指定记录为何失败并将问题解
	决.
	==========================================================================*/
	long SmtAdoRecordSet::GetStatus()
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->GetStatus();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetStatus 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		} 
		return -1;
	}

	/*========================================================================
	Name:		获取当前记录集中记录数目
	==========================================================================*/
	long SmtAdoRecordSet::GetRecordCount()
	{
		assert(m_pRecordset != NULL);
		try
		{
			long count = m_pRecordset->GetRecordCount();

			// 如果ado不支持此属性，则手工计算记录数目 --------
			if (count < 0)
			{
				long pos = GetAbsolutePosition();
				MoveFirst();
				count = 0;
				while (!IsEOF()) 
				{
					count++;
					MoveNext();
				}
				SetAbsolutePosition(pos);
			}
			return count;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetRecordCount 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		} 
	}

	/*========================================================================
	Name:		获取当前记录集中字段数目
	==========================================================================*/
	long SmtAdoRecordSet::GetFieldsCount()
	{
		assert(m_pRecordset != NULL);
		try
		{
			return GetFields()->Count;
		}
		catch(_com_error e)
		{
		//	TRACE(_T("Warning: GetFieldsCount 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		} 
	}
	/*========================================================================
	Name:		指示通过查询返回 Recordset 的记录的最大数目. 
	==========================================================================*/
	long SmtAdoRecordSet::GetMaxRecordCount()
	{
		assert(m_pRecordset != NULL);

		try
		{
			return m_pRecordset->GetMaxRecords();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetMaxRecordCount 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}
	}
	BOOL SmtAdoRecordSet::SetMaxRecordCount(long count)
	{
		assert(m_pRecordset != NULL);

		try
		{
			m_pRecordset->PutMaxRecords(count);
			return TRUE;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetMaxRecordCount 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	/*========================================================================
	Name:		指针是否在在记录集头
	==========================================================================*/
	BOOL SmtAdoRecordSet::IsBOF()
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->adoBOF;
		}
		catch(_com_error e)
		{
		//	TRACE(_T("Warning: IsBOF 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Name:		指针是否在在记录集尾
	==========================================================================*/
	BOOL SmtAdoRecordSet::IsEOF()
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->adoEOF;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: IsEOF 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	EditModeEnum SmtAdoRecordSet::GetEditMode()
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				return m_pRecordset->GetEditMode();
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: UpdateBatch 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return adEditNone;
		} 
		return	adEditNone; 
	}

	long SmtAdoRecordSet::GetPageCount()
	{
		assert(m_pRecordset != NULL);

		try
		{
			return m_pRecordset->GetPageCount();
		}
		catch (_com_error &e)
		{
		//	TRACE(_T("Warning: GetPageCount 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}
	}

	BOOL SmtAdoRecordSet::SetCacheSize(const long &lCacheSize)
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL && !(GetState() & adStateExecuting))
			{
				m_pRecordset->PutCacheSize(lCacheSize);
			}
		}
		catch (const _com_error& e)
		{
		//	TRACE(_T("Warning: SetCacheSize方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
		return TRUE;
	}

	long SmtAdoRecordSet::GetPageSize()
	{
		assert(m_pRecordset != NULL);

		try
		{
			return m_pRecordset->GetPageSize();
		}
		catch (_com_error &e)
		{
		//	TRACE(_T("Warning: GetPageCount 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}
	}

	/*========================================================================
	name:		指定当前记录所在的页.
	----------------------------------------------------------
	returns:	置或返回从 1 到 Recordset 对象 (PageCount) 所含页数的长整型
	值，或者返回以下常量. 
	[常量]			[说明]
	---------------------------------
	adPosUnknown	Recordset 为空，当前位置未知，或者提供者不支持 AbsolutePage 属性.  
	adPosBOF		当前记录指针位于 BOF(即 BOF 属性为 True).  
	adPosEOF		当前记录指针位于 EOF(即 EOF 属性为 True).  
	==========================================================================*/
	BOOL SmtAdoRecordSet::SetAbsolutePage(int nPage)
	{
		assert(m_pRecordset != NULL);

		try
		{
			m_pRecordset->PutAbsolutePage((enum PositionEnum)nPage);		
			return TRUE;
		}
		catch(_com_error &e)
		{
		//	TRACE(_T("Warning: SetAbsolutePage 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	long SmtAdoRecordSet::GetAbsolutePage()
	{
		assert(m_pRecordset != NULL);

		try
		{
			return m_pRecordset->GetAbsolutePage();
		}
		catch(_com_error &e)
		{
			//TRACE(_T("Warning: GetAbsolutePage 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}
	}

	/*========================================================================
	name:		指定 Recordset 对象当前记录的序号位置. 
	----------------------------------------------------------
	returns:	设置或返回从 1 到 Recordset 对象 (PageCount) 所含页数的长整
	型值，或者返回以下常量. 
	[常量]			[说明]
	---------------------------------
	adPosUnknown	Recordset 为空，当前位置未知，或者提供者不支持 AbsolutePage 属性.  
	adPosBOF		当前记录指针位于 BOF(即 BOF 属性为 True).  
	adPosEOF		当前记录指针位于 EOF(即 EOF 属性为 True).  
	----------------------------------------------------------
	Remarks:		使用 AbsolutePosition 属性可根据其在 Recordset 中的序号
	位置移动到记录，或确定当前记录的序号位置. 提供者必须支持该属性的相应功
	能才能使用该属性. 
	同 AbsolutePage 属性一样，AbsolutePosition 从 1 开始，并在当前记录
	为 Recordset 中的第一个记录时等于 1. 从 RecordCount 属性可获得 Recordset 
	对象的总记录数. 
	设置 AbsolutePosition 属性时，即使该属性指向位于当前缓存中的记录，
	ADO 也将使用以指定的记录开始的新记录组重新加载缓存. CacheSize 属性决定
	该记录组的大小. 
	注意   不能将 AbsolutePosition 属性作为替代的记录编号使用. 删除前面
	的记录时，给定记录的当前位置将发生改变. 如果 Recordset 对象被重新查询或
	重新打开，则无法保证给定记录有相同的 AbsolutePosition. 书签仍然是保持和
	返回给定位置的推荐方式，并且在所有类型的 Recordset 对象的定位时是唯一的
	方式. 
	==========================================================================*/
	BOOL SmtAdoRecordSet::SetAbsolutePosition(int nPosition)
	{
		assert(m_pRecordset != NULL);

		try
		{
			m_pRecordset->PutAbsolutePosition((enum PositionEnum)nPosition);		
			return TRUE;
		}
		catch(_com_error &e)
		{
			//TRACE(_T("Warning: SetAbsolutePosition 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	long SmtAdoRecordSet::GetAbsolutePosition()
	{
		assert(m_pRecordset != NULL);

		try
		{
			return m_pRecordset->GetAbsolutePosition();
		}
		catch(_com_error &e)
		{
			//TRACE(_T("Warning: GetAbsolutePosition 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}
	}

	BOOL SmtAdoRecordSet::SetCursorLocation(CursorLocationEnum CursorLocation)
	{
		assert(m_pRecordset != NULL);
		try
		{
			m_pRecordset->PutCursorLocation(CursorLocation);
			return TRUE;
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: PutCursorLocation 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	CursorLocationEnum SmtAdoRecordSet::GetCursorLocation()
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->GetCursorLocation();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetCursorLocation 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return adUseNone;
		}
	}

	BOOL SmtAdoRecordSet::SetCursorType(CursorTypeEnum CursorType)
	{
		assert(m_pRecordset != NULL);
		try
		{
			m_pRecordset->PutCursorType(CursorType);
			return TRUE;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: SetCursorType 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	CursorTypeEnum SmtAdoRecordSet::GetCursorType()
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->GetCursorType();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetCursorType 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return adOpenUnspecified;
		}
	}

	/*========================================================================
	Remarks:	Recordset 对象包括 Field 对象组成的 Fields 集合. 每个Field
	对象对应 Recordset 集中的一列.
	==========================================================================*/
	FieldsPtr SmtAdoRecordSet::GetFields()
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->GetFields();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetFields 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return NULL;
		} 
		return NULL;
	}

	/*========================================================================
	Remarks:	取得指定列字段的字段名.
	==========================================================================*/
	const char * SmtAdoRecordSet::GetFieldName(long lIndex)
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->Fields->GetItem(_variant_t(lIndex))->GetName();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetFieldName 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
		}
		return "";
	}

	/*========================================================================
	name:		取得 Field 对象一项或多项属性.
	----------------------------------------------------------
	returns:	对于 Field 对象, Attributes 属性为只读, 其值可能为以下任意
	一个或多个 FieldAttributeEnum 值的和.
	[常量]				[说明] 
	-------------------------------------------
	adFldMayDefer			指示字段被延迟, 即不从拥有整个记录的数据源检索
	字段值, 仅在显式访问这些字段时才进行检索. 
	adFldUpdatable		指示可以写入该字段. 
	adFldUnknownUpdatable 指示提供者无法确定是否可以写入该字段. 
	adFldFixed			指示该字段包含定长数据. 
	adFldIsNullable		指示该字段接受 Null 值. 
	adFldMayBeNull		指示可以从该字段读取 Null 值. 
	adFldLong				指示该字段为长二进制字段. 并指示可以使用 AppendChunk 
	和 GetChunk 方法. 
	adFldRowID			指示字段包含持久的行标识符, 该标识符无法被写入
	并且除了对行进行标识(如记录号、唯一标识符等)外不
	存在有意义的值. 
	adFldRowVersion		指示该字段包含用来跟踪更新的某种时间或日期标记. 
	adFldCacheDeferred	指示提供者缓存了字段值, 并已完成随后对缓存的读取. 
	==========================================================================*/
	long SmtAdoRecordSet::GetFieldAttributes(long lIndex)
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->Fields->GetItem(_variant_t(lIndex))->GetAttributes();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetFieldAttributes 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}
	}

	long SmtAdoRecordSet::GetFieldAttributes(LPCTSTR lpszFieldName)
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->Fields->GetItem(_variant_t(lpszFieldName))->GetAttributes();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetFieldAttributes 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}
	}

	/*========================================================================
	Name:		指示 Field 对象所定义的长度.
	----------------------------------------------------------
	returns:	返回某个字段定义的长度(按字节数)的长整型值.
	----------------------------------------------------------
	Remarks:	使用 DefinedSize 属性可确定 Field 对象的数据容量.
	==========================================================================*/
	long SmtAdoRecordSet::GetFieldDefineSize(long lIndex)
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->Fields->GetItem(_variant_t(lIndex))->DefinedSize;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetDefineSize 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}
	}

	long SmtAdoRecordSet::GetFieldDefineSize(LPCTSTR lpszFieldName)
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->Fields->GetItem(_variant_t(lpszFieldName))->DefinedSize;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetDefineSize 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}
	}

	/*========================================================================
	Name:	取得字段的值的实际长度.
	----------------------------------------------------------
	returns:	返回长整型值.某些提供者允许设置该属性以便为 BLOB 数据预留
	空间, 在此情况下默认值为 0.
	----------------------------------------------------------
	Remarks:	使用 ActualSize 属性可返回 Field 对象值的实际长度.对于所有
	字段,ActualSize 属性为只读.如果 ADO 无法确定 Field 对象值的实
	际长度, ActualSize 属性将返回 adUnknown.
	如以下范例所示, ActualSize 和  DefinedSize 属性有所不同: 
	adVarChar 声明类型且最大长度为 50 个字符的 Field 对象将返回为 
	50 的 DefinedSize 属性值, 但是返回的 ActualSize 属性值是当前记
	录的字段中存储的数据的长度.
	==========================================================================*/
	long SmtAdoRecordSet::GetFieldActualSize(long lIndex)
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->Fields->GetItem(_variant_t(lIndex))->ActualSize;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetFieldActualSize 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}	
	}

	long SmtAdoRecordSet::GetFieldActualSize(LPCTSTR lpszFieldName)
	{
		assert(m_pRecordset != NULL);
		try
		{
			return m_pRecordset->Fields->GetItem(_variant_t(lpszFieldName))->ActualSize;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetFieldActualSize 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		}	
	}

	/*========================================================================
	returns:	返回下列值之一. 相应的 OLE DB 类型标识符在下表的说明栏的括
	号中给出.
	[常量]			[说明] 
	---------------------------------------------------------
	adArray			与其他类型一起加入逻辑 OR 以指示该数据是那种类型的
	安全数组 (DBTYPE_ARRAY). 
	adBigInt			8 字节带符号的整数 (DBTYPE_I8). 
	adBinary			二进制值 (DBTYPE_BYTES). 
	adBoolean			布尔型值 (DBTYPE_BOOL). 
	adByRef			与其他类型一起加入逻辑 OR 以指示该数据是其他类型数
	据的指针 (DBTYPE_BYREF). 
	adBSTR			以空结尾的字符串 (Unicode) (DBTYPE_BSTR). 
	adChar			字符串值 (DBTYPE_STR). 
	adCurrency		货币值 (DBTYPE_CY).货币数字的小数点位置固定、小数
	点右侧有四位数字.该值保存为 8 字节范围为10,000 的带符
	号整型值. 
	adDate			日期值 (DBTYPE_DATE).日期按双精度型数值来保存, 数
	字全部表示从 1899 年 12 月 30 开始的日期数.小数部分是
	一天当中的片段时间. 
	adDBDate			日期值 (yyyymmdd) (DBTYPE_DBDATE). 
	adDBTime			时间值 (hhmmss) (DBTYPE_DBTIME). 
	adDBTimeStamp		时间戳 (yyyymmddhhmmss 加 10 亿分之一的小数)(DBTYPE_DBTIMESTAMP). 
	adDecimal			具有固定精度和范围的精确数字值 (DBTYPE_DECIMAL). 
	adDouble			双精度浮点值 (DBTYPE_R8). 
	adEmpty			未指定值 (DBTYPE_EMPTY). 
	adError			32 - 位错误代码 (DBTYPE_ERROR). 
	adGUID			全局唯一的标识符 (GUID) (DBTYPE_GUID). 
	adIDispatch		OLE 对象上 Idispatch 接口的指针 (DBTYPE_IDISPATCH). 
	adInteger			4 字节的带符号整型 (DBTYPE_I4). 
	adIUnknown		OLE 对象上 IUnknown 接口的指针 (DBTYPE_IUNKNOWN).
	adLongVarBinary	长二进制值. 
	adLongVarChar		长字符串值. 
	adLongVarWChar	以空结尾的长字符串值. 
	adNumeric			具有固定精度和范围的精确数字值 (DBTYPE_NUMERIC). 
	adSingle			单精度浮点值 (DBTYPE_R4). 
	adSmallInt		2 字节带符号整型 (DBTYPE_I2). 
	adTinyInt			1 字节带符号整型 (DBTYPE_I1). 
	adUnsignedBigInt	8 字节不带符号整型 (DBTYPE_UI8). 
	adUnsignedInt		4 字节不带符号整型 (DBTYPE_UI4). 
	adUnsignedSmallInt 2 字节不带符号整型 (DBTYPE_UI2). 
	adUnsignedTinyInt 1 字节不带符号整型 (DBTYPE_UI1). 
	adUserDefined		用户定义的变量 (DBTYPE_UDT). 
	adVarBinary		二进制值. 
	adVarChar			字符串值. 
	adVariant			自动变体型 (DBTYPE_VARIANT). 
	adVector			与其他类型一起加入逻辑 OR 中, 指示数据是 DBVECTOR 
	结构(由 OLE DB 定义).该结构含有元素的计数和其他类型 
	(DBTYPE_VECTOR) 数据的指针. 
	adVarWChar		以空结尾的 Unicode 字符串. 
	adWChar			以空结尾的 Unicode 字符串 (DBTYPE_WSTR). 
	----------------------------------------------------------
	Remarks:	返回指定字段的数据类型.
	==========================================================================*/
	DataTypeEnum SmtAdoRecordSet::GetFieldType(long lIndex)
	{
		assert(m_pRecordset != NULL);
		try 
		{
			return m_pRecordset->Fields->GetItem(_variant_t(lIndex))->GetType();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetField 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return adEmpty;
		}	
	}

	DataTypeEnum SmtAdoRecordSet::GetFieldType(LPCTSTR lpszFieldName)
	{
		assert(m_pRecordset != NULL);
		try 
		{
			return m_pRecordset->Fields->GetItem(_variant_t(lpszFieldName))->GetType();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetField发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return adEmpty;
		}	
	}

	BOOL SmtAdoRecordSet::IsFieldNull(LPCTSTR lpFieldName)
	{
		try
		{
			_variant_t vt = m_pRecordset->Fields->GetItem(lpFieldName)->Value;
			return (vt.vt == VT_NULL);
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: IsFieldNull 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	BOOL SmtAdoRecordSet::IsFieldNull(long index)
	{
		try
		{
			_variant_t vt = m_pRecordset->Fields->GetItem(_variant_t(index))->Value;
			return (vt.vt == VT_NULL);
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: IsFieldNull 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	/*========================================================================
	Name:	取得指定列的字段对象的指针.	
	==========================================================================*/
	FieldPtr SmtAdoRecordSet::GetField(long lIndex)
	{
		try
		{
			return GetFields()->GetItem(_variant_t(lIndex));
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetField发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return NULL;
		}
	}

	FieldPtr SmtAdoRecordSet::GetField(LPCTSTR lpszFieldName)
	{
		try
		{
			return GetFields()->GetItem(_variant_t(lpszFieldName));
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetField发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return NULL;
		}
	}

	/*########################################################################
	------------------------------------------------
	设置字段的值
	------------------------------------------------
	########################################################################*/
	BOOL SmtAdoRecordSet::PutCollect(long index, const _variant_t &value)
	{
		assert(m_pRecordset != NULL);
		assert(index < GetFieldsCount());
		try
		{
			if (m_pRecordset != NULL) 
			{
				m_pRecordset->PutCollect(_variant_t(index), value);
				return	TRUE;
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: PutCollect 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return	FALSE;
	}

	BOOL SmtAdoRecordSet::PutCollect(LPCSTR strFieldName, const _variant_t &value)
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL) 
			{
				m_pRecordset->put_Collect(_variant_t(strFieldName), value);
				return TRUE;
			}
		}
		catch (_com_error e)
		{
			return FALSE;
		//	TRACE(_T("Warning: PutCollect 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
		} 
		return	FALSE;
	}

	BOOL SmtAdoRecordSet::PutCollect(long index, const bool &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(index) != adBoolean);
	//		TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(index, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(LPCTSTR strFieldName, const bool &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(strFieldName) != adBoolean);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(strFieldName, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(long index, const BYTE &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(index) != adUnsignedTinyInt);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(index, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(LPCTSTR strFieldName, const BYTE &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(strFieldName) != adUnsignedTinyInt);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(strFieldName, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(long index, const short &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(index) != adSmallInt);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(index, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(LPCTSTR strFieldName, const short &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(strFieldName) != adSmallInt);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(strFieldName, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(long index, const int &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(index) != adInteger);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(index, _variant_t(long(value)));
	}

	BOOL SmtAdoRecordSet::PutCollect(LPCTSTR strFieldName, const int &value)
	{
		assert(m_pRecordset != NULL);

#ifdef _DEBUG
		if (GetFieldType(strFieldName) != adInteger);
	//		TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(strFieldName, _variant_t(long(value)));
	}

	BOOL SmtAdoRecordSet::PutCollect(long index, const long &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(index) != adBigInt);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(index, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(LPCTSTR strFieldName, const long &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(strFieldName) != adBigInt);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(strFieldName, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(long index, const DWORD &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(index) != adUnsignedBigInt);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		/*_variant_t var;
		var.vt = VT_UI4;
		var.ulVal = value;*/
		return PutCollect(index,_variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(LPCTSTR strFieldName, const DWORD &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(strFieldName) != adUnsignedBigInt);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		/*_variant_t var;
		var.vt = VT_UI4;
		var.ulVal = value;*/
		return PutCollect(strFieldName,_variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(long index, const float &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(index) != adSingle);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(index, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(LPCTSTR strFieldName, const float &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(strFieldName) != adSingle);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(strFieldName, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(long index, const double &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(index) != adDouble);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(index, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(LPCTSTR strFieldName, const double &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (GetFieldType(strFieldName) != adDouble);
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		return PutCollect(strFieldName, _variant_t(value));
	}

	BOOL SmtAdoRecordSet::PutCollect(long index, const string &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (! (GetFieldType(index) == adVarChar
			|| GetFieldType(index) == adChar
			|| GetFieldType(index) == adLongVarChar
			|| GetFieldType(index) == adVarWChar
			|| GetFieldType(index) == adWChar
			|| GetFieldType(index) == adLongVarWChar));
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

		/*_variant_t vt;
		vt.vt = (value.length() == 0) ? VT_NULL : VT_BSTR;
		vt.bstrVal = value.c_str();*/
		return PutCollect(index,_bstr_t(value.c_str()));
	}

	BOOL SmtAdoRecordSet::PutCollect(LPCTSTR strFieldName, const string &value)
	{
		assert(m_pRecordset != NULL);
#ifdef _DEBUG
		if (! (GetFieldType(strFieldName) == adVarChar
			|| GetFieldType(strFieldName) == adChar
			|| GetFieldType(strFieldName) == adLongVarChar
			|| GetFieldType(strFieldName) == adVarWChar
			|| GetFieldType(strFieldName) == adWChar
			|| GetFieldType(strFieldName) == adLongVarWChar));
		//	TRACE(_T("Warning: 你要存贮的字段与变量的数据类型不符; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
#endif

	/*	_variant_t vt;
		vt.vt = (value.length() == 0) ? VT_NULL : VT_BSTR;
		vt.bstrVal = value.c_str();*/
		return PutCollect(strFieldName,_bstr_t(value.c_str()));
	}


	/*########################################################################
	------------------------------------------------
	读取字段的值
	------------------------------------------------
	########################################################################*/
	BOOL SmtAdoRecordSet::GetCollect(long index,  bool &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = vartobool(m_pRecordset->GetCollect(_variant_t(index)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = false;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(LPCSTR strFieldName,  bool &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = vartobool(m_pRecordset->GetCollect(_variant_t(strFieldName)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = false;
			return FALSE;
		} 	
	}


	BOOL SmtAdoRecordSet::GetCollect(long index,  BYTE &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = vartoby(m_pRecordset->GetCollect(_variant_t(index)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(LPCSTR strFieldName,  BYTE &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = vartoby(m_pRecordset->GetCollect(_variant_t(strFieldName)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(long index,  short &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = vartoi(m_pRecordset->GetCollect(_variant_t(index)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(LPCSTR strFieldName,  short &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = vartoi(m_pRecordset->GetCollect(_variant_t(strFieldName)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(long index,  int &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = (int)vartol(m_pRecordset->GetCollect(_variant_t(index)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
		return FALSE;
	}

	BOOL SmtAdoRecordSet::GetCollect(LPCSTR strFieldName,  int &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = (int)vartol(m_pRecordset->GetCollect(_variant_t(strFieldName)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(long index,  long &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = vartol(m_pRecordset->GetCollect(_variant_t(index)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(LPCSTR strFieldName,  long &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = vartol(m_pRecordset->GetCollect(_variant_t(strFieldName)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
		return FALSE;
	}

	BOOL SmtAdoRecordSet::GetCollect(long index,  DWORD &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			_variant_t result = m_pRecordset->GetCollect(_variant_t(index));
			switch (result.vt)
			{
			case VT_UI4:
			case VT_I4:
				value = result.ulVal;
				break;
			case VT_NULL:
			case VT_EMPTY:
				value = 0;
				break;
			default:
			//	TRACE(_T("Warning: 无法读取相应的字段, 数据类型不匹配; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
				return FALSE;
			}		
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(LPCSTR strFieldName,  DWORD &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			_variant_t result = m_pRecordset->GetCollect(_variant_t(strFieldName));
			switch (result.vt)
			{
			case VT_UI4:
			case VT_I4:
				value = result.ulVal;
				break;
			case VT_NULL:
			case VT_EMPTY:
				value = 0;
				break;
			default:
			//	TRACE(_T("Warning: 无法读取相应的字段, 数据类型不匹配; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
				return FALSE;
			}		
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(long index,  float &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			_variant_t result = m_pRecordset->GetCollect(_variant_t(index));
			switch (result.vt)
			{
			case VT_R4:
				value = result.fltVal;
				break;
			case VT_UI1:
			case VT_I1:
				value = result.bVal;
				break;
			case VT_UI2:
			case VT_I2:
				value = result.iVal;
				break;
			case VT_NULL:
			case VT_EMPTY:
				value = 0;
				break;
			default:
			//	TRACE(_T("Warning: 无法读取相应的字段, 数据类型不匹配; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
				return FALSE;
			}		
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(LPCSTR strFieldName,  float &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			_variant_t result = m_pRecordset->GetCollect(_variant_t(strFieldName));
			switch (result.vt)
			{
			case VT_R4:
				value = result.fltVal;
				break;
			case  VT_R8:
				value = result.dblVal;
				break;
			case VT_UI1:
			case VT_I1:
				value = result.bVal;
				break;
			case VT_UI2:
			case VT_I2:
				value = result.iVal;
				break;
			case VT_NULL:
			case VT_EMPTY:
				value = 0;
				break;
			default:
			//	TRACE(_T("Warning: 无法读取相应的字段, 数据类型不匹配; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
				return FALSE;
			}		
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(long index,  double &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = vartof(m_pRecordset->GetCollect(_variant_t(index)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(LPCSTR strFieldName,  double &value)
	{
		assert(m_pRecordset != NULL);

		try
		{
			value = vartof(m_pRecordset->GetCollect(_variant_t(strFieldName)));
			return TRUE;
		}
		catch (_com_error e)
		{
			value = 0;
			return FALSE;
		} 	
	}

	BOOL SmtAdoRecordSet::GetCollect(long index, string& strValue)
	{
		assert(m_pRecordset != NULL);
		if (index < 0 || index >= GetFieldsCount())
		{
			return FALSE;
		}

		try
		{
			if (!IsOpen())
			{
			//	MessageBox(NULL, _T("数据库可能已经断开,\r\n请重新连接、然后重试."), _T("提示"), MB_ICONINFORMATION);
				return FALSE;
			} 
			if (m_pRecordset->adoEOF)
			{
				return FALSE;
			}
			_variant_t value = m_pRecordset->GetCollect(_variant_t(index));
			strValue = vartostr(value);
			return TRUE;
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: 字段访问失败. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}

		return FALSE;
	}

	BOOL SmtAdoRecordSet::GetCollect(LPCTSTR strFieldName, char * pValue,int nBufSize)
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (!IsOpen())
			{
			//	MessageBox(NULL, _T("数据库可能已经断开,\r\n请重新连接、然后重试."), _T("提示"), MB_ICONINFORMATION);
				return FALSE;
			} 
			if (m_pRecordset->adoEOF)
			{
				return FALSE;
			}
			_variant_t value = m_pRecordset->GetCollect(_variant_t(LPCTSTR(strFieldName)));
			snprintf( pValue, nBufSize, "%s", vartostr(value));	 
			return TRUE;	
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: 字段访问失败. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}

		return FALSE;
	}

	/*########################################################################
	------------------------------------------------
	将数据追加到大型文本、二进制数据 Field 对象. 
	------------------------------------------------
	########################################################################*/
	BOOL SmtAdoRecordSet::AppendChunk(FieldPtr pField, LPVOID lpData, UINT nBytes)
	{
		SAFEARRAY FAR *pSafeArray = NULL;
		SAFEARRAYBOUND rgsabound[1];

		try
		{
			rgsabound[0].lLbound = 0;
			rgsabound[0].cElements = nBytes;
			pSafeArray = SafeArrayCreate(VT_UI1, 1, rgsabound);

			for (long i = 0; i < (long)nBytes; i++)
			{
				UCHAR &chData	= ((UCHAR*)lpData)[i];
				HRESULT hr = SafeArrayPutElement(pSafeArray, &i, &chData);
				if (FAILED(hr))	return FALSE;
			}

			_variant_t varChunk;
			varChunk.vt = VT_ARRAY | VT_UI1;
			varChunk.parray = pSafeArray;

			return (pField->AppendChunk(varChunk) == S_OK);
		}
		catch (_com_error &e)
		{
		//	TRACE(_T("Warning: AppendChunk 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	BOOL SmtAdoRecordSet::AppendChunk(long index, LPVOID lpData, UINT nBytes)
	{
		assert(m_pRecordset != NULL);
		assert(lpData != NULL);
		if (adFldLong & GetFieldAttributes(index))
		{
			return AppendChunk(GetField(index), lpData, nBytes);
		}
		else return FALSE;
	}

	BOOL SmtAdoRecordSet::AppendChunk(LPCSTR strFieldName, LPVOID lpData, UINT nBytes)
	{
		assert(m_pRecordset != NULL);
		assert(lpData != NULL);
		if (adFldLong & GetFieldAttributes(strFieldName))
		{
			return AppendChunk(GetField(strFieldName), lpData, nBytes);
		}
		else return FALSE;
	}

	BOOL SmtAdoRecordSet::AppendChunk(long index, LPCTSTR lpszFileName)
	{
		assert(m_pRecordset != NULL);
		assert(lpszFileName != NULL);
		BOOL bret = FALSE;
		if (adFldLong & GetFieldAttributes(index))
		{
			/*FILE file;
			if (file.Open(lpszFileName, CFile::modeRead))
			{
				long length = (long)file.GetLength();
				char *pbuf = new char[length];
				if (pbuf != NULL && file.Read(pbuf, length) == (DWORD)length)
				{
					bret = AppendChunk(GetField(index), pbuf, length);
				}
				if (pbuf != NULL) delete[] pbuf;
			}
			file.Close();*/
		}
		return bret;
	}

	BOOL SmtAdoRecordSet::AppendChunk(LPCSTR strFieldName, LPCTSTR lpszFileName)
	{
		assert(m_pRecordset != NULL);
		assert(lpszFileName != NULL);
		BOOL bret = FALSE;
		if (adFldLong & GetFieldAttributes(strFieldName))
		{
		/*	CFile file;
			if (file.Open(lpszFileName, CFile::modeRead))
			{
				long length = (long)file.GetLength();
				char *pbuf = new char[length];
				if (pbuf != NULL && file.Read(pbuf, length) == (DWORD)length)
				{
					bret = AppendChunk(GetField(strFieldName), pbuf, length);
				}
				if (pbuf != NULL) delete[] pbuf;
			}
			file.Close();*/
		}
		return bret;
	}

	BOOL SmtAdoRecordSet::GetChunk(FieldPtr pField, LPVOID lpData)
	{
		assert(pField != NULL);
		assert(lpData != NULL);

		UCHAR chData;
		long index = 0;

		while (index < pField->ActualSize)
		{ 
			try
			{
				_variant_t varChunk = pField->GetChunk(100);
				if (varChunk.vt != (VT_ARRAY | VT_UI1))
				{
					return FALSE;
				}

				for (long i = 0; i < 100; i++)
				{
					if (SUCCEEDED( SafeArrayGetElement(varChunk.parray, &i, &chData) ))
					{
						((UCHAR*)lpData)[index] = chData;
						index++;
					}
					else
					{
						break;
					}
				}
			}
			catch (_com_error e)
			{
			//	TRACE(_T("Warning: GetChunk 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL SmtAdoRecordSet::GetChunk(long index, LPVOID lpData)
	{
		if (adFldLong & GetFieldAttributes(index))
			return  GetChunk(GetField(index), lpData);
		else return FALSE;
	}

	BOOL SmtAdoRecordSet::GetChunk(LPCSTR strFieldName, LPVOID lpData)
	{
		if (adFldLong & GetFieldAttributes(strFieldName))
			return  GetChunk(GetField(strFieldName), lpData);
		else return FALSE;
	}

	/*########################################################################
	------------------------------------------------
	其他方法
	------------------------------------------------
	########################################################################*/

	_RecordsetPtr SmtAdoRecordSet::operator =(_RecordsetPtr &pRecordSet)
	{
		Close();
		if (pRecordSet != NULL)
		{
			m_pRecordset = NULL;
			m_pRecordset = pRecordSet;
			return m_pRecordset;
		}
		return NULL;
	}

	/*========================================================================
	Name:	确定指定的 Recordset 对象是否支持特定类型的功能.	
	----------------------------------------------------------
	Params:	CursorOptions   长整型, 包括一个或多个下列 CursorOptionEnum 值.
	[常量]				[说明] 
	------------------------------------
	adAddNew			可使用 AddNew 方法添加新记录. 
	adApproxPosition	可读取并设置 AbsolutePosition 和 AbsolutePage 的属性. 
	adBookmark			可使用 Bookmark 属性获得对特定记录的访问. 
	adDelete			可以使用 Delete 方法删除记录. 
	adHoldRecords		可以检索多个记录或者更改下一个检索位置而不必提交所
	有挂起的更改. 
	adMovePrevious		可使用 MoveFirst 和 MovePrevious 方法, 以及 Move 或
	GetRows 方法将当前记录位置向后移动而不必使用书签. 
	adResync			通过 Resync 方法, 使用在基本的数据库中可见的数据更
	新游标. 
	adUpdate			可使用 Update 方法修改现有的数据. 
	adUpdateBatch		可以使用批更新(UpdateBatch 和 CancelBatch 方法) 将
	更改组传输给提供者. 
	adIndex				可以使用 Index 属性命名索引. 
	adSeek				可以使用 Seek 方法定位 Recordset 中的行. 
	----------------------------------------------------------
	returns:	返回布尔型值, 指示是否支持 CursorOptions 参数所标识的所有功能.
	----------------------------------------------------------
	Remarks:	使用 Supports 方法确定 Recordset 对象所支持的功能类型. 如
	果 Recordset 对象支持其相应常量在 CursorOptions 中的功能, 那么 Supports
	方法返回 True.否则返回 False.
	注意   尽管 Supports 方法可对给定的功能返回 True, 但它不能保证提供者可
	以使功能在所有环境下均有效. Supports 方法只返回提供者是否支持指定的功能
	(假定符合某些条件). 例如, Supports 方法可能指示 Recordset 对象支持更新
	(即使游标基于多个表的合并), 但并且某些列仍然无法更新.
	==========================================================================*/
	BOOL SmtAdoRecordSet::Supports(CursorOptionEnum CursorOptions)
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (m_pRecordset != NULL)
			{
				return m_pRecordset->Supports(CursorOptions);
			}
		}
		catch (const _com_error& e)
		{
		//	TRACE(_T("Warning: Supports方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
		return FALSE;
	}

	/*========================================================================
	name:		为 Recordset 中的数据指定筛选条件.
	==========================================================================*/
	BOOL SmtAdoRecordSet::SetFilter(LPCTSTR lpszFilter)
	{
		assert(m_pRecordset != NULL);
		assert(IsOpen());

		try
		{
			m_pRecordset->PutFilter(lpszFilter);
			return TRUE;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: SetFilter 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	/*========================================================================
	name:		为 Recordset 中的数据指定排序条件.
	==========================================================================*/
	BOOL SmtAdoRecordSet::SetSort(LPCTSTR lpszCriteria)
	{
		assert(IsOpen());

		try
		{
			m_pRecordset->PutSort(lpszCriteria);
			return TRUE;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: SetFilter 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	/*========================================================================
	name:		返回唯一标识 Recordset 对象中当前记录的书签。
	==========================================================================*/
	_variant_t SmtAdoRecordSet::GetBookmark()
	{
		assert(m_pRecordset != NULL);
		try
		{
			if (IsOpen())
			{
				m_varBookmark = m_pRecordset->GetBookmark();
				return m_varBookmark;
			}		
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetBookmark 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
		}
		return _variant_t((long)adBookmarkFirst);
	}

	/*========================================================================
	name:		将 Recordset 对象的当前记录设置为由有效书签所标识的记录。
	==========================================================================*/
	BOOL SmtAdoRecordSet::SetBookmark(_variant_t varBookMark)
	{
		assert(m_pRecordset != NULL);

		try
		{
			if (IsOpen() && varBookMark.vt != VT_EMPTY && varBookMark.vt != VT_NULL)
			{
				m_pRecordset->PutBookmark(varBookMark);
				return TRUE;
			}	
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: SetBookmark 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
		}
		return FALSE;
	}

	void SmtAdoRecordSet::SetAdoConnection(SmtAdoConnection *pConnection)
	{
		m_pConnection = pConnection;
	}

	_RecordsetPtr& SmtAdoRecordSet::GetRecordset()
	{
		return m_pRecordset;
	}

	const char * SmtAdoRecordSet::GetLastError()
	{
		assert(m_pConnection != NULL);
		return m_pConnection->GetLastErrorText();
	}

	/*========================================================================
	name:	创建与现有 Recordset 对象相同的复制 Recordset 对象。可选择指定
	该副本为只读。
	==========================================================================*/
	BOOL SmtAdoRecordSet::Clone(SmtAdoRecordSet &pRecordSet)
	{
		assert(m_pRecordset != NULL);

		try
		{
			pRecordSet = m_pRecordset->Clone(adLockUnspecified);
			return TRUE;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: Clone 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	/*========================================================================
	name:		搜索 Recordset 中满足指定标准的记录. 如果满足标准，则记录集
	位置设置在找到的记录上，否则位置将设置在记录集的末尾. 
	----------------------------------------------------------
	params:		[criteria]:   字符串，包含指定用于搜索的列名、比较操作符和
	值的语句. 
	[searchDirection]:    可选的 SearchDirectionEnum 值，指定搜
	索应从当前行还是下一个有效行开始. 其值可为 adSearchForward 或 adSearchBackward. 
	搜索是在记录集的开始还是末尾结束由 searchDirection 值决定. 
	[常量]			[说明]
	---------------------------------
	adSearchForward 	
	adSearchBackward	
	----------------------------------------------------------
	Remarks:	criteria 中的"比较操作符"可以是">"(大于)、"<"(小于)、"="(等
	于)、">="(大于或等于)、"<="(小于或等于)、"<>"(不等于)或"like"(模式匹配).  
	criteria 中的值可以是字符串、浮点数或者日期. 字符串值以单引号分界(如
	"state = 'WA'"). 日期值以"#"(数字记号)分界(如"start_date > #7/22/97#"). 
	如"比较操作符"为"like"，则字符串"值"可以包含"*"(某字符可出现一次或
	多次)或者"_"(某字符只出现一次). (如"state like M_*"与 Maine 和 Massachusetts 
	匹配.). 
	==========================================================================*/
	BOOL SmtAdoRecordSet::Find(LPCTSTR lpszFind, SearchDirectionEnum SearchDirection)
	{
		assert(m_pRecordset != NULL);
	
		try
		{
			if (strcmp(lpszFind,"") != 0)
			{
				m_strFind = lpszFind;
			}

			if (m_strFind.length() == 0) return FALSE;

			m_pRecordset->Find(_bstr_t(m_strFind.c_str()), 0, SearchDirection, "");

			if ((IsEOF() || IsBOF()) )
			{
				return FALSE;
			}
			else
			{
				m_SearchDirection = SearchDirection;
				GetBookmark();
				return TRUE;
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: Find 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	/*========================================================================
	name:		查找下一条满足条件的记录.
	==========================================================================*/
	BOOL SmtAdoRecordSet::FindNext()
	{
		assert(m_pRecordset != NULL);

		try
		{
			if (m_strFind.length() == 0) return FALSE;

			m_pRecordset->Find(_bstr_t(m_strFind.c_str()), 1, m_SearchDirection, m_varBookmark);

			if ((IsEOF() || IsBOF()) )
			{
				return FALSE;
			}
			else
			{
				GetBookmark();
				return TRUE;
			}
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: FindNext 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	BOOL SmtAdoRecordSet::RecordBinding(CADORecordBinding &pAdoRecordBinding)
	{
		m_pAdoRecordBinding = NULL;

		try
		{
			if (SUCCEEDED(m_pRecordset->QueryInterface(__uuidof(IADORecordBinding), (LPVOID*)&m_pAdoRecordBinding)))
			{
				if (SUCCEEDED(m_pAdoRecordBinding->BindToRecordset(&pAdoRecordBinding)))
				{
					return TRUE;
				}	
			}
			return TRUE;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: RecordBinding 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}
	}

	BOOL SmtAdoRecordSet::AddNew(CADORecordBinding &pAdoRecordBinding)
	{
		try
		{
			if (m_pAdoRecordBinding->AddNew(&pAdoRecordBinding) == S_OK)
			{
				m_pAdoRecordBinding->Update(&pAdoRecordBinding);
				return TRUE;
			}
			return FALSE;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: AddNew 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		}	
	}

	/*========================================================================
	Name:	获得当前游标位置
	-----------------------------------------------------
	Remarks: 返回位置。
	==========================================================================*/
	long SmtAdoRecordSet::GetCursorPos()
	{
		long count = m_pRecordset->GetRecordCount();
		while (!m_pRecordset->adoEOF)
		{
			m_pRecordset->MoveNext();
			count--;
		}
		m_pRecordset->MoveFirst();
		m_pRecordset->Move(count);
		return count;
	}


	/*========================================================================
	Name:	获得指定列在集合中的位置。
	-----------------------------------------------------
	params:	filedName   列名
	---------------------------------
	==========================================================================*/

	int SmtAdoRecordSet::GetFieldIndex(const char * filedName)
	{
		long colCount = GetFieldsCount();
		int i;
		for (i = 0 ;i < colCount; i++)
		{
			if (strcmp(filedName, GetFieldName(i)) == 0)
			{
				break;
			}
		}
		return i == colCount ?-1:i;
	}

	BOOL SmtAdoRecordSet::LoadOLEDataFromDB(const char *  CreateFileName ,const char *  feildName)
	{
		long nSize = GetFields()->GetItem(_variant_t(feildName))->ActualSize;
		if(nSize > 0)
		{
			_variant_t	varBLOB;
			varBLOB = GetFields()->GetItem(_variant_t(feildName))->GetChunk(nSize);
			//if(varBLOB.vt == (VT_ARRAY | VT_UI1))
			//{
			//	CFile file;
			//	if (!file.Open(CreateFileName,CFile::modeWrite|CFile::modeCreate))
			//		return FALSE;
			//	//			if(BYTE *pBuffer = new BYTE [nSize+1])		///重新申请必要的存储空间
			//	//			{	
			//	char *pBuf = NULL;
			//	SafeArrayAccessData(varBLOB.parray,(void **)&pBuf);
			//	//				memcpy(pBuffer,pBuf,nSize);				///复制数据到缓冲区m_pBMPBuffer
			//	SafeArrayUnaccessData (varBLOB.parray);
			//	//	int nSize = lDataSize;
			//	//				(Pic->LoadPictureData(pBuffer, nSize));
			//	file.Write((void*) pBuf,nSize);
			//	file.Close();
			//	//				delete [] pBuffer;
			//	pBuf=0;

			//	//			}
			//}
		}
		return TRUE;
	}
}