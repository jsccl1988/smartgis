# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Task 3: base/sys cutover renames (export macros + snake_case).

$ErrorActionPreference = 'Stop'
$root = 'C:/Dev/src/gis/smartgis/src'

function Get-TreeFiles($tree) {
    Get-ChildItem -Path (Join-Path $root $tree) -Recurse -File |
        Where-Object { $_.Extension -match '^\.(h|hpp|c|cc|cpp|inl|inc|gn)$' }
}

function Apply-Renames($files, [hashtable]$map) {
    $sorted = $map.Keys | Sort-Object { $_.Length } -Descending
    foreach ($f in $files) {
        $text = [IO.File]::ReadAllText($f.FullName)
        $orig = $text
        foreach ($k in $sorted) {
            $v = [regex]::Escape($k)
            $text = [regex]::Replace($text, "\b$v\b", $map[$k])
        }
        if ($text -ne $orig) {
            [IO.File]::WriteAllText($f.FullName, $text)
            Write-Host "updated $($f.FullName)"
        }
    }
}

# snake_case renames (longest keys first via sort)
$snake = @{
    'GetSingletonPtr' = 'get_singleton_ptr'
    'DestoryInstance' = 'destroy_instance'
    'RegisterListenerMsg' = 'register_listener_msg'
    'UnRegisterListenerMsg' = 'unregister_listener_msg'
    'RemoveAllListener' = 'remove_all_listener'
    'RegisterListener' = 'register_listener'
    'RemoveListener' = 'remove_listener'
    'GetActiveListener' = 'get_active_listener'
    'SetActiveListener' = 'set_active_listener'
    'GetListenerCount' = 'get_listener_count'
    'AppendFuncItems' = 'append_func_items'
    'GetFuncItems' = 'get_func_items'
    'RegisterMsg' = 'register_msg'
    'UnRegisterMsg' = 'unregister_msg'
    'UnRegister' = 'unregister'
    'SetActive' = 'set_active'
    'GetDefaultLog' = 'get_default_log'
    'SetDefaultLog' = 'set_default_log'
    'DestroyAllLog' = 'destroy_all_log'
    'DestroyLog' = 'destroy_log'
    'CreateLog' = 'create_log'
    'SetLogDir' = 'set_log_dir'
    'LogMessage' = 'log_message'
    'LoadAllPlugin' = 'load_all_plugin'
    'UnLoadAllPlugin' = 'unload_all_plugin'
    'StartAllPlugin' = 'start_all_plugin'
    'StopAllPlugin' = 'stop_all_plugin'
    'LoadPlugin' = 'load_plugin'
    'UnLoadPlugin' = 'unload_plugin'
    'SetDefaultStyle' = 'set_default_style'
    'GetDefaultStyle' = 'get_default_style'
    'DestroyAllStyle' = 'destroy_all_style'
    'DestroyStyle' = 'destroy_style'
    'CreateStyle' = 'create_style'
    'GetStyleCount' = 'get_style_count'
    'FindStyleNameIndex' = 'find_style_name_index'
    'AddStyle' = 'add_style'
    'RemoveStyle' = 'remove_style'
    'GetStyleName' = 'get_style_name'
    'GetStyleType' = 'get_style_type'
    'SetStyleType' = 'set_style_type'
    'SetStyleName' = 'set_style_name'
    'SetPenDesc' = 'set_pen_desc'
    'SetBrushDesc' = 'set_brush_desc'
    'SetAnnoDesc' = 'set_anno_desc'
    'SetSymbolDesc' = 'set_symbol_desc'
    'GetPenDesc' = 'get_pen_desc'
    'GetBrushDesc' = 'get_brush_desc'
    'GetAnnoDesc' = 'get_anno_desc'
    'GetSymbolDesc' = 'get_symbol_desc'
    'ViewportToRect' = 'viewport_to_rect'
    'WindowportToRect' = 'windowport_to_rect'
    'EnvelopeToRect' = 'envelope_to_rect'
    'RectToEnvelope' = 'rect_to_envelope'
    'AnnoDescToLogFont' = 'anno_desc_to_log_font'
    'LogFontToAnnoDesc' = 'log_font_to_anno_desc'
    'GetSysStyleConfig' = 'get_sys_style_config'
    'SetSysStyleConfig' = 'set_sys_style_config'
    'GetSysMapDocInfo' = 'get_sys_map_doc_info'
    'SetSysMapDocInfo' = 'set_sys_map_doc_info'
    'GetSysPrjInfo' = 'get_sys_prj_info'
    'SetSysPrjInfo' = 'set_sys_prj_info'
    'GetSysPra' = 'get_sys_pra'
    'SetSysPra' = 'set_sys_pra'
    'VarToString' = 'var_to_string'
    'VarToDouble' = 'var_to_double'
    'VarToInteger' = 'var_to_integer'
    'VarToByte' = 'var_to_byte'
    'VarToBool' = 'var_to_bool'
    'VarToLong' = 'var_to_long'
    'IntegerListToString' = 'integer_list_to_string'
    'StringListToString' = 'string_list_to_string'
    'RealListToString' = 'real_list_to_string'
    'STR_Tokenize' = 'str_tokenize'
    'STR_Duplicate' = 'str_duplicate'
    'STR_Count' = 'str_count'
    'IsEqual' = 'is_equal'
    'SplitFileName' = 'split_file_name'
    'GetParentDictory' = 'get_parent_directory'
    'GetAppPath' = 'get_app_path'
    'GetAppTempPath' = 'get_app_temp_path'
    'CreateAllPathDirectory' = 'create_all_path_directory'
    'DeleteDirectory' = 'delete_directory'
    'GetTempName' = 'get_temp_name'
    'lRectTofRect' = 'l_rect_to_f_rect'
    'fRectTolRect' = 'f_rect_to_l_rect'
    'AjustlRect' = 'adjust_l_rect'
    'AjustfRect' = 'adjust_f_rect'
    'IsInfRect' = 'is_in_f_rect'
    'IsInlRect' = 'is_in_l_rect'
    'GetInterpColor' = 'get_interp_color'
    'GetRandomColor' = 'get_random_color'
    'CreateListenerMenu' = 'create_listener_menu'
    'AppendListenerMenu' = 'append_listener_menu'
    'GetImageTypeByFileExt' = 'get_image_type_by_file_ext'
    'ClearFileInfos' = 'clear_file_infos'
    'SearchCurrentDir' = 'search_current_dir'
    'GetFileInfos' = 'get_file_infos'
    'SetCurrentDir' = 'set_current_dir'
    'GetCurrentDir' = 'get_current_dir'
    'UpDir' = 'up_dir'
    'ScanDir' = 'scan_dir'
    'SetReceiver' = 'set_receiver'
    'Unexecute' = 'unexecute'
    'AddCommand' = 'add_command'
    'DeleteCommand' = 'delete_command'
    'CallCommand' = 'call_command'
    'ClearAllCommands' = 'clear_all_commands'
    'CanUndo' = 'can_undo'
    'CanRedo' = 'can_redo'
    'PushUndoCommand' = 'push_undo_command'
    'PopUndoCommand' = 'pop_undo_command'
    'PushRedoCommand' = 'push_redo_command'
    'PopRedoCommand' = 'pop_redo_command'
    'DeleteUndoCommands' = 'delete_undo_commands'
    'DeleteRedoCommands' = 'delete_redo_commands'
    'GetThread' = 'get_thread'
    'GetThreadID' = 'get_thread_id'
    'SetState' = 'set_state'
    'GetPriority' = 'get_priority'
    'SetPriority' = 'set_priority'
    'TryLock' = 'try_lock'
    'SafeChange' = 'safe_change'
    'GetJobNo' = 'get_job_no'
    'SetJobNo' = 'set_job_no'
    'GetJobName' = 'get_job_name'
    'SetJobName' = 'set_job_name'
    'GetWorkThread' = 'get_work_thread'
    'SetWorkThread' = 'set_work_thread'
    'SetThreadPool' = 'set_thread_pool'
    'GetThreadPool' = 'get_thread_pool'
    'IsWorking' = 'is_working'
    'GetIdleThread' = 'get_idle_thread'
    'AppendToIdleList' = 'append_to_idle_list'
    'MoveToBusyList' = 'move_to_busy_list'
    'MoveToIdleList' = 'move_to_idle_list'
    'CreateThread' = 'create_thread'
    'DeleteThread' = 'delete_thread'
    'SetMaxNum' = 'set_max_num'
    'GetMaxNum' = 'get_max_num'
    'SetAvailLowNum' = 'set_avail_low_num'
    'GetAvailLowNum' = 'get_avail_low_num'
    'SetAvailHighNum' = 'set_avail_high_num'
    'GetAvailHighNum' = 'get_avail_high_num'
    'GetActualAvailNum' = 'get_actual_avail_num'
    'GetAllNum' = 'get_all_num'
    'GetIdleNum' = 'get_idle_num'
    'GetBusyNum' = 'get_busy_num'
    'SetNormalNum' = 'set_normal_num'
    'GetNormalNum' = 'get_normal_num'
    'TerminateAll' = 'terminate_all'
    'GetCurUsedSize' = 'get_cur_used_size'
    'GetMemPoolUnitSize' = 'get_mem_pool_unit_size'
    'GetMemory' = 'get_memory'
    'FreeMemory' = 'free_memory'
    'CheckToRelease' = 'check_to_release'
    'ProcessLog' = 'process_log'
    'FreeAllMemPool' = 'free_all_mem_pool'
    'GetIdentifier' = 'get_identifier'
    'AddTail' = 'add_tail'
    'ClearMemPoolMgr' = 'clear_mem_pool_mgr'
    'GetMemPool' = 'get_mem_pool'
    'IsInit' = 'is_init'
    'Intersects' = 'intersects'
    'Contains' = 'contains'
    'IsInstalled' = 'is_installed'
    'StartCtrlDispatcher' = 'start_ctrl_dispatcher'
    'UserRun' = 'user_run'
    'UserCtrl' = 'user_ctrl'
    'LogEvent' = 'log_event'
    'ServiceMain' = 'service_main'
    'ServiceCtrl' = 'service_ctrl'
    'PulseEvent' = 'pulse_event'
    'ResetEvent' = 'reset_event'
    'SetEvent' = 'set_event'
    'GetName' = 'get_name'
    'SetName' = 'set_name'
    'GetMsgs' = 'get_msgs'
    'AppendMsg' = 'append_msg'
    'Notify' = 'notify'
    'Register' = 'register_'
    'Execute' = 'execute'
    'Action' = 'action'
    'Clone' = 'clone'
    'Merge' = 'merge'
    'Intersect' = 'intersect'
    'Height' = 'height'
    'Width' = 'width'
    'Wakeup' = 'wakeup'
    'SetJob' = 'set_job'
    'GetJob' = 'get_job'
}

foreach ($tree in @('base', 'sys')) {
    Apply-Renames (Get-TreeFiles $tree) $snake
}

# Export macro token updates
$coreFiles = Get-TreeFiles 'base' | Where-Object { $_.FullName -notmatch '[\\/]style[\\/]' -and $_.FullName -notmatch '[\\/]ipc[\\/]' }
$styleFiles = Get-TreeFiles 'base' | Where-Object { $_.FullName -match '[\\/]style[\\/]' }
$sysFiles = Get-TreeFiles 'sys'

foreach ($f in $coreFiles) {
    $t = [IO.File]::ReadAllText($f.FullName)
    $n = $t
    $n = $n.Replace('SMT_EXPORT_CLASS', 'CORE_EXPORT')
    $n = $n.Replace('SMT_EXPORT_API', 'CORE_EXPORT')
    $n = $n.Replace('#if !defined(CORE_EXPORT)', '#if !defined(CORE_EXPORTS)')
    if ($n -ne $t) { [IO.File]::WriteAllText($f.FullName, $n); Write-Host "export core $($f.Name)" }
}
foreach ($f in $styleFiles) {
    $t = [IO.File]::ReadAllText($f.FullName)
    $n = $t
    $n = $n.Replace('SMT_EXPORT_CLASS', 'STYLE_EXPORT')
    $n = $n.Replace('SMT_EXPORT_API', 'STYLE_EXPORT')
    $n = $n.Replace('#if !defined(CORE_EXPORT)', '#if !defined(STYLE_EXPORTS)')
    $n = $n.Replace('#if !defined(STYLE_EXPORT)', '#if !defined(STYLE_EXPORTS)')
    if ($n -ne $t) { [IO.File]::WriteAllText($f.FullName, $n); Write-Host "export style $($f.Name)" }
}
foreach ($f in $sysFiles) {
    $t = [IO.File]::ReadAllText($f.FullName)
    $n = $t
    $n = $n.Replace('SMT_EXPORT_CLASS', 'SYS_EXPORT')
    $n = $n.Replace('#if !defined(SYS_EXPORT)', '#if !defined(SYS_EXPORTS)')
    if ($n -ne $t) { [IO.File]::WriteAllText($f.FullName, $n); Write-Host "export sys $($f.Name)" }
}

Write-Host 'done'
