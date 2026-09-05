# mqtt protocal
Created 星期五 07 五月 2021


gdscript interface
------------------

### g_Authorize
 login(String usr,String psw)
 is_logged_in(String usr)
 logout(String usr)
 getUser()
 getAllUsers()
 getAllUsersCount()
 getDisabledUsers()
 getLockedUsers()
 newUser(String usr,String psw, String comment, bool enable, bool lock)
 modifyUser(String usr, String comment, bool enable, bool lock)
 deleteUser(String usr)
// when old_psw=*MD1588501# 时不验证旧密码，强行修改为新密码
 modifyUserPassword(String usr, String old_psw, String new_psw)
 lockUser(String usr)
 unlockUser(String usr)
 isUserDisabled(String usr)
 isUserLocked(String usr)
 getAllGroups()
 getAllGroupsCount()
 newGroup(String grp)
 deleteGroup(String grp)
 modifyGroup(String grp)
 getUsersInGroup(String grp)
 addUserToGroup(String usr, String grp)
 removeUserFromGroup(String usr, String grp)
 isUserInGroup(String usr, String grp)
Authorize(String usr,String psw) // 认证



#### MDObjectType
__根据对象类型写入数据库日志表中的Type类型 对应 "@OT"__
目前系统默认使用的类型为 **0, 2, 3, 9, 999**
enum class MDObjectType
{
**null_obj = 0,**

project             = 1,
**variable            = 2,**
**window            = 3,**
timer                = 4,
recipe               = 5,
logGroup         = 6,
alarmGroup    = 7,
historyGroup   = 8,
**action               = 9,**
event                = 10,
account             = 11,
powerGroup  = 12,
alarm               = 13,
alarmLevel      = 14,
alarmCondition   = 15,
mdswitch               = 16,

**custom		  = 999**
};

#### MDActionType
__根据动作类型生成日志记录的记录模式__
enum class MDActionType
{
null_action                              = 0,

// 1 - 32
reserved                                 = 1,

// 33 - 40
project_Load                         = 33,
project_Unload                    = 34,
project_ExitFS                     = 35,

// 41 - 56
variable_WriteValue               = 41,
variable_ReadValue                = 42,

// 57 - 64
timer_Delay                       = 57,
timer_Start                         = 58,
timer_Stop                        = 59,

// 65 - 80
recipe_Upload                 = 65,
recipe_Download           = 66,
recipe_View                     = 67,
recipe_Modify                   = 68,
recipe_Delete                   = 69,

// 81 - 96
window_Open                 = 81,
window_Close                 = 82,
window_Move                 = 83,
window_Hide                 = 84,
window_RunScript            = 85,
window_ShowByPid            = 86,
window_HideByPid            = 87,
window_CloseByPid           = 88,

// 97 - 104
account_Modify                 = 97,
account_ModifyPassword = 98,
account_Delete                 = 99,

// 105 - 112
powerGroup_Modify        = 105,
powerGroup_Delete         = 106,

// 113 - 144
action_Execute           = 113,
alarm_Confirm           = 114,
historyGroup_Save     = 115,
tts_Play                        = 116,
account_powergroup_Manage = 117,
alarm_Acknowledge           = 118
};

int CMDSecurityPolicyExpert::accountSignature(QString actionInfo, QString sigConf)
{

// 1. 运行时当前实时签名信息
//    {
//        "account": "u1",
*        "password": "", *optional 动作保护时可不填密码
*        "actionType": 0, * 动作类型,系统预定义(如 打开窗口，复位报警 ...) 需要与设计时保存的保持一致，否则报动作类型不匹配的错误 参考 enum MDActionType
*        "customAction": "XXX", * 动作999 对应的描述
*        "customObjectName": "XXX", * 对象999 对应的对象名
*        "srcState": "XXX", * 当改变变量值时 的原始值
*        "destState": "XXX", * 当改变变量值时 的目标值
*        "actionComment": "YYY", * 本次签名备注
*        "customComment": "ZZZ", * 用户自定义信息 若此项不为空则日志记录按照自定义信息为准，而不使用系统标准记录模式
*obsolete      "actionSrc": "XXX", * 便于最后形成 日志信息(如 某某某 yyyy-MM-dd HH:mm:ss 打开窗口 XXX)， 按照系统标准记录方式记录， 此项非空按照此项设置替换动作对象名并记录至数据库，对应字段为Name
*        "protectMode": 0 * 0-动作保护 1-电子签名(操作人) 2-电子签名(校验人)
//    }
// 2. 设计时签名配置
// if 动作保护
// else 电子签名
//    {
*        "@EL": true, * 启用日志记录
//        "@OKI": {
//            "@Comment": "",
*            "@N": "w", * 动作名 , 当类型是用户自定义类型999时后台以此作为动作名记录，其余为预定义动作情况，无需填此值  eg:执行动作: @N (@Comment) [签名成功]
*            "@OT": 9, * 对象类型 MDObjectType 999为自定义对象
*            "@T": 113 * 动作类型 MDActionType 999为自定义动作
//        },
*        "@PM": 2, * 保护模式 0-无 1-动作保护 2-电子签名
*        "@ST": 2, * 签名类型 0-无 1-operator_only 2-operator_then_verifier
*        "WorkerGroupArray": [  * 操作组
*            "*", * *组代表系统所有组
//            "Manufacture",
//            "Engineers"
//        ],
*        "InspectorGroupArray": [   * 校验组
//            "Administrators",
//            "Engineers"
//        ]
//    }

### g_Formula
new(String path)
copy(String src, String dest)
append(String formula_path, String var_path)
remove(String path)
clear(String formula_path)
set(String formula_path, String var_path, Variant var_value)
#  var_path = '*' return all
#  var_path = '['iovar.varname1', 'iovar.varname2']'  return 2 var value value1,value2
get(String formula_path, String var_path)
Dictionary query(String formula_path)
has()
download
upload


 addItem(QString,QString,QString)
 deleteItem(QString,QString)
 setItemValue(QString,QString,QString)
 getItemValue(QString,QString)
 create(QString)
 copy(QString src, QString dest)
 remove(QString)
 removeDir(QString)
 exists(QString)
 existsDir(QString)
 isRecipe(QString)
 enumRecipe(QString path)
 toFileName(QString)
 toDirName(QString)
 toAbsoluteName(QString)
 toRelativeName(QString)
 toFullFileName(QString)
 treeItemType(QString)
 download(QString)
 downloadMemoryRecipe(QString,QScriptValue,QScriptValue)
 downloadMemoryRecipe_ItemsCount(QString,QScriptValue,int)
 recipeDownloadPara(QString,QScriptValue)
 upload(QString)
 downloadDir(QString)
 downloadGroup(QString,QScriptValue)
 modifyRecipe(QString)
 showRecipeManagerDialog()
 itemsToJsonString(QString)
 jsonStringToItems(QString,QString)
 versionInfo(QString,QScriptValue)
 upgradeMajorVersion(QString)
 fire_beginDownloading(QString,int,QString)
 fire_downloaded(QString,DownloadFinishFlag,QString)
 fire_beginDownloadingGroup(QString,QStringList)
 fire_endDownloadingGroup(QString,QStringList,QStringList)


Alarm 通知信息
----------
const _demo = {
"AID": "A15",
"AckState":  null ,
"AckTime":  null ,
"AckTimeMS":  null ,
"AlarmLevel": "Alarm",
"BackColor": 4294111786,
"Comment": "è½¬å­è¯å«éè¯¯",
"ConfirmState":  null ,
"ConfirmTime":  null ,
"ConfirmTimeMS":  null ,
"CurrentUser": "root",
"EndTime":  null ,
"EndTimeMS":  null ,
"EndValue":  null ,
"ForeColor": 4294901760,
"ID": "è½¬å­è¯å«éè¯¯_2024-05-14 11:12:13.115",
"Message": "è½¬å­è¯å«éè¯¯",
"Name": "è½¬å­è¯å«éè¯¯",
"Serverity": 0,
"ShelveState":  null ,
"ShelveTime":  null ,
"ShelveTimeMS":  null ,
"StartTime": "2024-05-14T11:12:13.115",
"StartTimeMS": 115,
"StartValue": "true",
"Status": 0,
"SuppressState":  null ,
"SuppressTime":  null ,
"SuppressTimeMS":  null ,
"SuppressedOrShelved":  null
}

mqtt req/rsp interface
----------------------

### 兼容多设备连接
req示例
req/g_Variable/readVariable/[clientid]

rsp示例
rsp/g_Variable/readVariable/[clientid]


### 非法的请求回馈
{
"msg": "Unknown request topic: req/g_Variable/writeVariableSwitch01",
"result": "0"
}

### rsp/auto
按照规则自动处理回馈topic
默认将前3个字母**req**替换为**rsp**



*****


### g_Variable


*****



#### req/g_Variable/readVariable
{
  "name": "virtual.D.D4000"
}

#### rsp/g_Variable/readVariable
{
"result": "2:754"
}
结果模板
type:value

#### req/g_Variable/writeVariable
{
  "name": "virtual.D.D4000",
  "value": "265"
}

#### rsp/g_Variable/writeVariable
{
"result": "1"
}

#### req/g_Variable/writeVariableWithoutSig
{
  "name": "virtual.D.D4000",
  "value": "265"
}

#### rsp/g_Variable/writeVariableWithoutSig
{
"result": "1"
}

#### req/g_Variable/writeVariableRelative
{
  "name": "virtual.D.D4000",
  "relativeValue": "3"
}

#### rsp/g_Variable/writeVariableRelative
{
"result": "1"
}

#### req/g_Variable/writeVariableRelativeWithoutSig
{
  "name": "virtual.D.D4000",
  "relativeValue": "-2"
}

#### rsp/g_Variable/writeVariableRelativeWithoutSig
{
"result": "1"
}

#### req/g_Variable/writeVariableSwitch01
{
  "name": "virtual.Alarm.M1502"
}

#### rsp/g_Variable/writeVariableSwitch01
{
"result": "1"
}


#### req/g_Variable/writeVariableSwitch01WithoutSig
{
  "name": "virtual.Alarm.M1502"
}

#### rsp/g_Variable/writeVariableSwitch01WithoutSig
{
"result": "1"
}


#### req/g_Variable/writeVariables
{
  "names": ["virtual.D.D4000","virtual.D.D4012"],
  "values": ["456","5"]
}

#### rsp/g_Variable/writeVariables
{
"result": "1"
}

#### req/g_Variable/writeVariablesWithoutSig
{
  "names": ["virtual.D.D4000","virtual.D.D4012"],
  "values": ["789","10"]
}

#### rsp/g_Variable/writeVariablesWithoutSig
{
"result": "1"
}



*****


### g_Window


*****



#### req/g_Window/open
// pwid>0 打开指定窗口（未打开的新窗口）或者 显示已经打开的窗口，并指定父窗口pwid
// pwid=0 时则按照 x, y,w,h 设置位置及尺寸
// pwid<0 重新打开指定窗口，并指定父窗口pwid
{
  "name": "hmi/log",
  "pwid": 0,
  "x": 20,
  "y": 30,
  "w": 500,
  "h": 300
}

#### rsp/g_Window/open
{
"result": "1"
}

#### req/g_Window/isOpen
{
  "name": "hmi/log"
}

#### rsp/g_Window/isOpen
{
"result": "1"
}

#### req/g_Window/move
{
  "name": "HMI/log",
  "left": 30,
  "top": 50
}

#### rsp/g_Window/move
{
"result": "1"
}

#### req/g_Window/callJsFunc

# enum QUERYMODE {TIME, BATCH}
# enum QMode {
# 	AUTO,#初始化的查询
# 	MANUAL#手动点击的查询
# }

{
  "name": "hmi/log",
  "funcName": "query",
  "params":
  {
 "mode": 0,
 "qmode": 0,
 "st": "2024-08-30 00:00:00",
 "et": "2024-09-02 23:59:59",
 "lang": "en"
  }
}

##### 查询报告
Modules/g_Window/callJsFunc	inputs:	["hmi/log", "query", "{\"et\":\"2025-02-28 17:50:57\",\"lang\":\"zh_CN\",\"mode\":0,\"qmode\":1,\"st\":\"2025-02-28 00:00:00\"}"]

##### 导出pdf
//        The permissions section can include one or more of the following features:

//        Printing – Top Quality Printing
//        DegradedPrinting – Lower Quality Printing
//        ModifyContents – Also allows Assembly
//        Assembly
//        CopyContents – Also allows ScreenReaders
//        ScreenReaders
//        ModifyAnnotations – Also allows FillIn
//        FillIn
//        AllFeatures – Allows the user to perform all of the above, and top quality printing.


1. gd ua 调用方法

invokeMethodAsync	Modules/g_Window/callJsFunc	["hmi/log", "exportToPdf", "\"C:/temp/usb/log_20250228000000_20250228175057.pdf\",\"5566\",\"Printing\""]

2. uaexpert 调用方法

![](./mqtt_protocal_files/cheats/MindSCADA/new/pasted_image004.png)

3. mqttx调用方法

3.1 ~~exportToPdf 失败~~
{
  "name": "hmi/alarm",
  "funcName": "exportToPdf",
  "params":"\"C:/5555.pdf\",\"5566\",\"Printing\""
}

3.2 export_pdf 成功
{
  "name": "hmi/alarm",
  "funcName": "export_pdf",
  "params": {
"pdfPath": "C:/5555.pdf",
"password": "5566",
"perm": "Printing"
}
}

{
  "name": "hmi/log",
  "funcName": "export_pdf",
  "params": {
"pdfPath": "/home/pi/5555.pdf",
"password": "5566",
"perm": "Printing"
}
}

##### 导出txt格式报告
模板文件tpl如果未给，或者给空串"" ,表示使用默认tpl文件，即项目文件夹Report下与报表设计文件同名的tpl文件
![](./mqtt_protocal_files/cheats/MindSCADA/new/pasted_image005.png)

1. gd ua 调用方法

invokeMethodAsync	Modules/g_Window/callJsFunc	["hmi/batch", "exportToTxt", "\"C:/batch_20250228175057.txt\",\"C:/batchReport.tpl\""]

2. uaexpert 调用方法

![](./mqtt_protocal_files/cheats/MindSCADA/new/pasted_image006.png)


3. mqttx调用方法

export_txt 成功
3.1 按照指定的模板文件导出
{
  "name": "hmi/batch",
  "funcName": "export_txt",
  "params": {
"txtPath": "C:/5555.txt",
"tplPath": "d:/xxx/yyy/zzz.tpl"
}
}

3.2 按照默认的模板文件导出
{
  "name": "hmi/batch",
  "funcName": "export_txt",
  "params": {
"txtPath": "C:/5555.txt"
}
}

#### rsp/g_Window/callJsFunc
{
"result": "1"
}


#### req/g_Window/hide
{
  "name": "HMI/log",
}


#### rsp/g_Window/hide
{
"result": "1"
}


#### req/g_Window/hideByPid
{
  "pid": 35626,
  "phwnd": 0
}


#### rsp/g_Window/hideByPid
{
"result": "1"
}


#### req/g_Window/showByPid
// 吸附指定进程的主窗体
// phwnd = 0 表示全局置顶
// phwnd > 0 设置phwnd窗口句柄为父窗体
{
  "pid": 35626,
  "phwnd": 0
}


#### rsp/g_Window/showByPid
{
"result": "1"
}


#### req/g_Window/restore
// 送回已经吸附出来的窗体回原进程
{
  "name": "HMI/log",
}


#### rsp/g_Window/restore
{
"result": "1"
}


#### req/g_Window/hideRuntime
{}


#### rsp/g_Window/hideRuntime
{
"result": "1"
}


#### req/g_Window/showRuntime
{}


#### rsp/g_Window/showRuntime
{
"result": "1"
}


#### req/g_Window/hideRecipeRuntime
{}

#### rsp/g_Window/hideRecipeRuntime
{
"result": "1"
}

#### req/g_Window/showRecipeRuntime
{}

#### rsp/g_Window/showRecipeRuntime
{
"result": "1"
}

#### req/g_Window/getWid
{
  "name": "HMI/log",
}

#### rsp/g_Window/getWid
{
"result": 1377760
}

#### req/g_Window/close
{
  "name": "HMI/log",
}

#### rsp/g_Window/close
{
"result": "1"
}

#### req/g_Window/closeByPid
{
  "pid": 35626,
  "phwnd": 0
}

#### rsp/g_Window/closeByPid
{
"result": "1"
}

#### req/g_Window/enterFullScreen
{}

#### rsp/g_Window/enterFullScreen
{
"result": "1"
}

#### req/g_Window/exitFullScreen
{}

#### rsp/g_Window/exitFullScreen
{
"result": "1"
}




*****


### g_Authorize


*****



#### req/g_Authorize/accountSignature

// 1. 运行时当前实时签名信息
//    {
//        "account": "u1",
*        "password": "", *optional 动作保护时可不填密码
*        "actionType": 0, * 动作类型,系统预定义(如 打开窗口，复位报警 ...) 需要与设计时保存的保持一致，否则报动作类型不匹配的错误 参考 enum MDActionType
*        "customAction": "XXX", * 动作999 对应的描述
*        "customObjectName": "XXX", * 对象999 对应的对象名
*        "srcState": "XXX", * 当改变变量值时 的原始值
*        "destState": "XXX", * 当改变变量值时 的目标值
*        "actionComment": "YYY", * 本次签名备注
*        "customComment": "ZZZ", * 用户自定义信息 若此项不为空则日志记录按照自定义信息为准，而不使用系统标准记录模式
*obsolete      "actionSrc": "XXX", * 便于最后形成 日志信息(如 某某某 yyyy-MM-dd HH:mm:ss 打开窗口 XXX)， 按照系统标准记录方式记录， 此项非空按照此项设置替换动作对象名并记录至数据库，对应字段为Name
*        "protectMode": 0 * 0-动作保护 1-电子签名(操作人) 2-电子签名(校验人)
//    }
// 2. 设计时签名配置
// if 动作保护
// else 电子签名
//    {
*        "@EL": true, * 启用日志记录
//        "@OKI": {
//            "@Comment": "",
*            "@N": "w", * 动作名 , 当类型是用户自定义类型999时后台以此作为动作名记录，其余为预定义动作情况，无需填此值  eg:执行动作: @N (@Comment) [签名成功]
*            "@OT": 9, * 对象类型 MDObjectType 999为自定义对象
*            "@T": 113 * 动作类型 MDActionType 999为自定义动作
//        },
*        "@PM": 2, * 保护模式 0-无 1-动作保护 2-电子签名
*        "@ST": 2, * 签名类型 0-无 1-operator_only 2-operator_then_verifier
*        "WorkerGroupArray": [  * 操作组
*            "*", * *组代表系统所有组
//            "Manufacture",
//            "Engineers"
//        ],
*        "InspectorGroupArray": [   * 校验组
//            "Administrators",
//            "Engineers"
//        ]
//    }



{
  "actionInfo": 
  {
"account": "1"
"password": "1",
"actionComment": "用户自定义信息",
"customComment": "用户自定义信息",
"actionType": 999,
"protectMode": 0
  },
  "sigConf": 
  {
"@EL": true,
"@OKI": 
{
"@Comment": "",
"@N": "动作名",
"@OT": 9,
"@T": 113
},
"@PM": 1,
"@ST": 1,
"WorkerGroupArray": ["Administrators", "Engineers","Operators","Manufacture","Guests","*"],
"InspectorGroupArray": ["Administrators", "Engineers","Operators","Manufacture","Guests","*"]
  }
}

#### rsp/g_Authorize/accountSignature

//! 安全验证结果
enum class MDSecurityResultCode
{
no_result                           = -1,  //! 失败
success                             = 0,   //! 成功
account_not_exist                   = 1,   //! 帐户不存在
invalid_account                     = 2,   //! 无效的账户
invalid_password                    = 3,   //! 密码错误
account_has_no_power                = 4,   //! 帐户不具有权限
account_disabled                    = 5,   //! 帐户被禁用
password_expired                    = 6,   //! 密码已过期,请及时修改密码
account_locked                      = 7,   //! 帐户被锁定,需要管理员手动解锁
action_type_not_match               = 8,   //! 验证动作类型与预设类型不匹配
password_not_match_strategy         = 9,   //! 密码不符合安全策略
cannot_use_oldpassword              = 10,  //! 不可使用旧密码
first_login_modify_password         = 11,  //! 第一次登陆必须修改密码
account_has_not_assign_to_power     = 12,  //! 帐户未分配至任何权限组
};


{
"result": 0
}


#### req/g_Authorize/accountsCount
{}

#### rsp/g_Authorize/accountsCount
{
"result": 10
}

#### req/g_Authorize/addAccountToGroup
{
  "account": "t",
  "group": "Operators"
}

#### rsp/g_Authorize/addAccountToGroup
{
"result": "1"
}

#### req/g_Authorize/resetAccountGroups
重置账户的组
{
  "account": "t",
  "group": ["Operators","QA"]
}

#### rsp/g_Authorize/resetAccountGroups
{
"result": "1"
}

#### req/g_Authorize/deleteAccount
{
  "name": "t"
}

#### rsp/g_Authorize/deleteAccount
-1 :  Account deletion not allowed
0:  fail
1:  succeed

{
"result": "1"
}

#### req/g_Authorize/deleteGroup
{
  "name": "Operators"
}

#### rsp/g_Authorize/deleteGroup
{
"result": "1"
}


#### req/g_Authorize/disableAccount
{
  "name": "t"
}

#### rsp/g_Authorize/disableAccount
{
"result": "1"
}


#### req/g_Authorize/disableGroup
{
  "name": "Operators"
}

#### rsp/g_Authorize/disableGroup
{
"result": "1"
}


#### req/g_Authorize/enableAccount
{
  "name": "t"
}

#### rsp/g_Authorize/enableAccount
{
"result": "1"
}

#### req/g_Authorize/enableGroup
{
  "name": "Operators"
}

#### rsp/g_Authorize/enableGroup
{
"result": "1"
}

#### req/g_Authorize/commit
{}

#### rsp/g_Authorize/commit
{
"result": "1"
}

#### req/g_Authorize/getAccount
{
  "name": "3",
}

#### rsp/g_Authorize/getAccount
// case 1: fail
{
"result": "0"
}

// case 2: succeed
{
"result" : {
"Comment": "",
"Enable": true,
"Locked": false,
"Name": "3",
"Password": "3",
"SPList": [
{
"@EL": 0,
"@OKI": {
"@Comment": "",
"@N": "",
"@OT": 11,
"@T": 97
},
"@PM": 0,
"@ST": 1,
"InspectorGroupArray": [
],
"WorkerGroupArray": [
]
},
{
"@EL": 0,
"@OKI": {
"@Comment": "",
"@N": "",
"@OT": 11,
"@T": 98
},
"@PM": 0,
"@ST": 1,
"InspectorGroupArray": [
],
"WorkerGroupArray": [
]
},
{
"@EL": 0,
"@OKI": {
"@Comment": "",
"@N": "",
"@OT": 11,
"@T": 99
},
"@PM": 0,
"@ST": 1,
"InspectorGroupArray": [
],
"WorkerGroupArray": [
]
}
],
"Type": 0,
"Visible": false
}
}

#### req/g_Authorize/getAccountGroups
{
  "name": "t"
}

#### rsp/g_Authorize/getAccountGroups
{
  "result": ["Operators","Engineers"]
}

#### req/g_Authorize/getAccountOption
{
  "name": "3"
}

#### rsp/g_Authorize/getAccountOption
// case 1: fail
{
"result": "0"
}

// case 2: succeed
{
"result" : {
}
}

#### req/g_Authorize/getAccounts
{}

#### rsp/g_Authorize/getAccounts
{
  "result": ["1","2","3"]
}

#### req/g_Authorize/getAccountsConf
{}

#### rsp/g_Authorize/getAccountsConf
{
  "result": [
{
"Comment": "",
"Enable": true,
"Locked": false,
"LockedRuntime": true,
"Name": "1",
"Password": "1",
"Groups": [
"Administrators"
],
"Type": 0,
"Visible": false
},
{
"Comment": "",
"Enable": true,
"Locked": false,
"LockedRuntime": false,
"Name": "2",
"Password": "2",
"Groups": [
"Engineers"
],
"Type": 0,
"Visible": false
}
]
}

#### req/g_Authorize/getAccountsInGroup
{
  "group": "Operators"
}


#### rsp/g_Authorize/getAccountsInGroup
{
  "result": ["1","t"]
}


#### req/g_Authorize/getCurrentAccount
{}


#### rsp/g_Authorize/getCurrentAccount
{
  "result": "1"
}


#### req/g_Authorize/getDisabledAccounts
{}


#### rsp/g_Authorize/getDisabledAccounts
{
  "result": ["u1","t"]
}


#### req/g_Authorize/getDisabledGroups
{}


#### rsp/g_Authorize/getDisabledGroups
{
  "group": ["Operators"]
}

{
  "group": []
}


#### req/g_Authorize/getGroup
{
  "name": "Operators"
}


#### rsp/g_Authorize/getGroup
{
  "result": 
{
}
}


#### req/g_Authorize/getGroups
{}


#### rsp/g_Authorize/getGroups
{
  "result": ["Administrators", "Engineers","Operators","Manufacture","Guests"]
}


#### req/g_Authorize/getLockedAccounts
{}


#### rsp/g_Authorize/getLockedAccounts
{
  "result": ["u1","t"]
}


#### req/g_Authorize/groupsCount
{}


#### rsp/g_Authorize/groupsCount
{
  "result": 5
}


#### req/g_Authorize/isAccountEnabled
{
  "name": "accountName"
}


#### rsp/g_Authorize/isAccountEnabled
{
  "result": "1"
}


#### req/g_Authorize/isAccountInGroup
{
  "account": "t",
  "group": "Operators"
}


#### rsp/g_Authorize/isAccountInGroup
{
  "result": "0"
}


#### req/g_Authorize/isAccountLocked
{
  "name": "3"
}


#### rsp/g_Authorize/isAccountLocked
{
  "result": "0"
}


#### req/g_Authorize/isAccountLogin
{
  "name": "3"
}


#### rsp/g_Authorize/isAccountLogin
{
  "result": "1"
}


#### req/g_Authorize/lockAccount
{
  "name": "3"
}


#### rsp/g_Authorize/lockAccount
{
  "result": "1"
}


#### req/g_Authorize/login
{
  "name": "3",
  "password": "3",
  "checkGroup": 0
}


#### rsp/g_Authorize/login
{
  "result": 1
}


#### req/g_Authorize/logout
{}


#### rsp/g_Authorize/logout
{
  "result": 1
}


#### req/g_Authorize/modifyPassword
{
  "name": "3",
  "oldPassword": "3",
  "newPassword": "xxxx"
}

**trick**
当旧密码为 __*MD1588501#__  时，可跳过旧密码验证直接修改为新密码



#### rsp/g_Authorize/modifyPassword
{
  "result": 1
}


#### req/g_Authorize/removeAccountFromGroup
{
  "account": "t",
  "group": "Operators"
}


#### rsp/g_Authorize/removeAccountFromGroup
{
  "result": "1"
}


#### req/g_Authorize/resetAccount
{
  "name": "t"
}

#### rsp/g_Authorize/resetAccount
{
  "result": "1"
}


#### req/g_Authorize/resetAllAccount
{}


#### rsp/g_Authorize/resetAllAccount
{
  "result": "1"
}


#### req/g_Authorize/setAccountOption
{
  "option":
{
"AutoUnlockPeriod": 30,
"AutoUnlockPeriodUnit": 0,
"CantUseOldPsw": true,
"ContainAlphabet": false,
"ContainExtraAlphabet": false,
"ContainNumber": false,
"ContainUppercaseLowercase": false,
"EnableDeleteAccount": false,
"EnableModifyPasswordFirstLogin": false,
"LockScreenPeriod": 30,
"LockScreenPeriodUnit": 0,
"MinAccountIDLength": 1,
"MinPasswordLength": 0,
"PasswordInvalidContinueInputCount": 0,
"PasswordValidPeriod": 0,
"PasswordValidPeriodUnit": 2,
"PswCannotContainAccout": false
}
}

#### rsp/g_Authorize/setAccountOption
{
  "result": "1"
}


#### req/g_Authorize/unlockAccount
{
  "name": "t"
}

#### rsp/g_Authorize/unlockAccount
{
  "result": "1"
}

#### req/g_Authorize/upsertAccount
name, comment, password, enable, lock
{
  "name": "t",
  "comment": "tcomment",
  "password": "tpsw",
  "enable": "1",
  "lock": "0",
}

#### rsp/g_Authorize/upsertAccount
{
  "result": "1"
}


#### req/g_Authorize/upsertGroup
{
  "name": "t",
  "comment": "tcomment",
  "enable": "1",
  "lock": "0"
}

#### rsp/g_Authorize/upsertGroup
{
  "result": "1"
}



*****


### g_Alarm


*****


#### req/g_Alarm/acknowledge
此处alarmId应为报警运行时 ID， 例如： 门盖未到位_2025-01-17 15:10:45.601
{
  "alarmId": "alrmid",
  "comment": "ack"
}


#### rsp/g_Alarm/acknowledge
{
  "result": "1"
}


#### req/g_Alarm/acknowledgeAll
{}


#### rsp/g_Alarm/acknowledgeAll
{
  "result": "1"
}


#### req/g_Alarm/addComment
{
  "alarmId": "alrmid",
  "comment": "ack"
}


#### rsp/g_Alarm/addComment
{
  "result": "1"
}


#### req/g_Alarm/confirm
{
  "alarmId": "alrmid",
  "comment": "confirm"
}


#### rsp/g_Alarm/confirm
{
  "result": "1"
}


#### req/g_Alarm/confirmAll
{}


#### rsp/g_Alarm/confirmAll
{
  "result": "1"
}


#### req/g_Alarm/getAlarmById
{
  "id": "alrmid"
}


#### rsp/g_Alarm/getAlarmById
{
  "result":
  {
  }
}


#### req/g_Alarm/getAlarmByIndex
{
  "index": 3
}


#### rsp/g_Alarm/getAlarmByIndex
{
  "result":
  {
  }
}


#### req/g_Alarm/getAlarmConfById
{
  "id": "alrmid"
}


#### rsp/g_Alarm/getAlarmConfById
{
  "result":
  {
  }
}


#### req/g_Alarm/getAlarmCount
{}


#### rsp/g_Alarm/getAlarmCount
{
  "result": 3
}


#### req/g_Alarm/shelve
{
  "alarmId": "alrmid",
  "comment": "shelve"
}


#### rsp/g_Alarm/shelve
{
  "result": "1"
}


#### req/g_Alarm/suppress
{
  "alarmId": "alrmid",
  "comment": "supress"
}


#### rsp/g_Alarm/suppress
{
  "result": "1"
}


*****


### g_Recipe

配方模块
__返回值说明__
成功
{
  "result": 0
}

失败
{
  "result": -1
}


*****


#### req/g_Recipe/copy
{
  "srcRecipeName": "template/demo.rcp",
  "destRecipeName": "usr/xxx.rcp"
}


#### rsp/g_Recipe/copy
{
  "result": 0
}


#### req/g_Recipe/create
{
  "recipeName": "usr/yyy.rcp"
}


#### rsp/g_Recipe/create
{
  "result": 0
}


#### req/g_Recipe/download
{
  "recipeName": "usr/xxx.rcp",
  "showProcessDialog": 1
}


#### rsp/g_Recipe/download
{
  "result": 0
}

#### req/g_Recipe/downloadMem
{
  "recipeName": "usr/xxx.rcp",
   "recipeContent": "配方文件内容完整json字符创"
  "showProcessDialog": 1
}


#### rsp/g_Recipe/downloadMem
{
  "result": 0
}


#### req/g_Recipe/enumRecipe
{
  "onlyRecipe": 0
}


#### rsp/g_Recipe/enumRecipe
{
  "result": ["usr", "usr/1.rcp", "usr/2.rcp"]
}


#### req/g_Recipe/exists
{
  "recipeName": "usr/xxx.rcp"
}


#### rsp/g_Recipe/exists
{
  "result": 0
}


#### req/g_Recipe/getContent
{
  "recipeName": "usr/xxx.rcp"
}


#### rsp/g_Recipe/getContent
{
  "result":
  [
  ]
}


#### req/g_Recipe/getRecipeCount
{
  "onlyRecipe": 0
}


#### rsp/g_Recipe/getRecipeCount
{
  "result": 10
}


#### req/g_Recipe/getRecipeName
{
  "index": 0
}


#### rsp/g_Recipe/getRecipeName
{
  "result": "usr/xxx.rcp"
}


#### req/g_Recipe/remove
{
  "recipeName": "usr/xxx.rcp"
}


#### rsp/g_Recipe/remove
{
  "result": 0
}


#### req/g_Recipe/save
{
  "recipeName": "usr/xxx.rcp",
  "content": 
  [
  ]
}


#### rsp/g_Recipe/save
{
  "result": 0
}


#### req/g_Recipe/toFileName
{
  "recipeName": "usr/xxx.rcp",
}


#### rsp/g_Recipe/toFileName
{
  "result": "c:/prj/Recipe/usr/xxx.rcp"
}


#### req/g_Recipe/upload
{
  "recipeName": "usr/xxx.rcp",
}


#### rsp/g_Recipe/upload
{
  "result": 0
}

#### req/g_Recipe/uploadMem
{
  "recipeName": "usr/xxx.rcp",
   "varPathArrStr": ["d1/d2/d3/v1","d1/d2/v2"]
}


#### rsp/g_Recipe/uploadMem
{
  "result": 0
}

### sync

#### req/syncFile
{
  "src": "**[%PRJPATH%]**/KV_bak/__var__",  // 此路径为svc服务所在路径，通配符**[%PRJPATH%]**表示当前svc运行项目所在目录
  "dst": "**[%PRJPATH%]/**__varmq__" // 此路径为scada client gd项目所在路径，通配符**[%PRJPATH%]**表示当前scada gd运行项目所在目录，或者发布包*.pck 所在目录
}

#### req/sync__var__
{}

#### req/sync__varcrc__
{}


### Event

#### Event/#

#### Recipe/#
{
"EventId": "0x4018e2b6468965948fdbcd9ca21ef44d",
"EventType": "MDRecipeEventType",
"LocalTime": "UTC+08:00",
"Message": "Status changed to 1.",
"ReceiveTime": "2024-08-29T08:11:24.999Z",
"Severity": 100,
"SourceName": "Server",
"Time": "2024-08-29T08:11:24.999Z",
"meta": [
"usr/多段速/11.rcp",
"39"
],
"status": 1
}

{
"EventId": "0x3a49417468094cb52a2b4f0b89034885",
"EventType": "MDRecipeEventType",
"LocalTime": "UTC+08:00",
"Message": "Status changed to 2.",
"ReceiveTime": "2024-08-29T08:11:22.162Z",
"Severity": 100,
"SourceName": "Server",
"Time": "2024-08-29T08:11:22.162Z",
"meta": [
"usr/多段速/11.rcp",
"2"
],
"status": 2
}

{
"EventId": "0xa27a0d73fcd72d9e9aa328a3f14d4032",
"EventType": "MDRecipeEventType",
"LocalTime": "UTC+08:00",
"Message": "Status changed to 99.",
"ReceiveTime": "2024-08-29T08:11:25.000Z",
"Severity": 100,
"SourceName": "Server",
"Time": "2024-08-29T08:11:25.000Z",
"meta": [
"usr/多段速/11.rcp",
"39",
"39"
],
"status": 99
}



#### Report/#
{
"EventType": "MDReportEventType",
"SourceName": "report1",
"Message": "control name:report1 report name:Log onPrintDone",
"Time": "2026-02-11 12:12:12",
"Status": 0,
"meta": {
"name": "Log",
"subEventName": "onPrintDone"
}
}



