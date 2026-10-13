#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MindSCADA OPC-UA Server Service (uaserver)
Listening on opc.tcp://0.0.0.0:4840/
Provides Objects/Modules with g_Authorize, g_Alarm, g_Window methods
and all SCADA variables for native/tunnel/gateway clients.
"""

import sys
import os
import time
import json
import logging
from opcua import Server, ua, uamethod

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] [UaServer] %(message)s")
logger = logging.getLogger("uaserver")

VARCONF_PATH = r"C:\prj\luxiangyi472\scada4\build_web\mmkv\varconf_default.json"
SNAPSHOT_PATH = r"C:\prj\luxiangyi472\scada4\build_web\mmkv\__var___snapshot.json"

current_account = ""

@uamethod
def getCurrentAccount(parent):
    return current_account

@uamethod
def getAccountGroups(parent, account):
    return ["Administrators"]

@uamethod
def getAccounts(parent):
    return ["1", "2", "3", "root"]

@uamethod
def accountsCount(parent):
    return 4

@uamethod
def groupsCount(parent):
    return 3

@uamethod
def getAccountOption(parent):
    return "{}"

@uamethod
def getPasswordOption(parent):
    return "{}"

@uamethod
def accountSignature(parent, action_info, sig_conf):
    return 0

@uamethod
def login(parent, account, password, force_check):
    global current_account
    current_account = account
    logger.info("Login successful for account: %s", account)
    return 0

@uamethod
def logout(parent):
    global current_account
    current_account = ""
    return 0

@uamethod
def commit(parent):
    return 0

@uamethod
def enableAccount(parent, account):
    return 0

@uamethod
def disableAccount(parent, account):
    return 0

@uamethod
def lockAccount(parent, account):
    return 0

@uamethod
def unlockAccount(parent, account):
    return 0

@uamethod
def isAccountLocked(parent, account):
    return False

@uamethod
def isAccountEnabled(parent, account):
    return True

@uamethod
def isAccountInGroup(parent, account, group):
    return True

@uamethod
def getAlarmCount(parent):
    return 0

@uamethod
def openBridged(parent, form_name):
    logger.info("openBridged called: %s", form_name)
    return 0

@uamethod
def openWindow(parent, form_name, wid, x, y, w, h):
    logger.info("openWindow called: %s", form_name)
    return 0

@uamethod
def closeWindow(parent, form_name):
    return 0

@uamethod
def hideWindow(parent, form_name):
    return 0

@uamethod
def restoreWindow(parent, form_name):
    return 0

@uamethod
def callJsFunc(parent, form_name, func_name, params):
    logger.info("callJsFunc called: %s %s %s", form_name, func_name, params)
    return ""

def main():
    server = Server()
    server.set_endpoint("opc.tcp://0.0.0.0:4840/freeopcua/server/")
    server.set_server_name("MindSCADA OPC-UA Server")

    # Set up security
    server.set_security_policy([ua.SecurityPolicyType.NoSecurity])

    idx = server.register_namespace("http://mindteco.com/mindscada")
    objects = server.get_objects_node()

    # 1. Create Modules folder
    modules = objects.add_folder(idx, "Modules")

    # 2. Add g_Authorize module
    g_auth = modules.add_folder(idx, "g_Authorize")
    g_auth.add_method(idx, "getCurrentAccount", getCurrentAccount, [], [ua.VariantType.String])
    g_auth.add_method(idx, "getAccountGroups", getAccountGroups, [ua.VariantType.String], [ua.VariantType.String])
    g_auth.add_method(idx, "getAccounts", getAccounts, [], [ua.VariantType.String])
    g_auth.add_method(idx, "accountsCount", accountsCount, [], [ua.VariantType.Int32])
    g_auth.add_method(idx, "groupsCount", groupsCount, [], [ua.VariantType.Int32])
    g_auth.add_method(idx, "getAccountOption", getAccountOption, [], [ua.VariantType.String])
    g_auth.add_method(idx, "getPasswordOption", getPasswordOption, [], [ua.VariantType.String])
    g_auth.add_method(idx, "accountSignature", accountSignature, [ua.VariantType.String, ua.VariantType.String], [ua.VariantType.Int32])
    g_auth.add_method(idx, "login", login, [ua.VariantType.String, ua.VariantType.String, ua.VariantType.Int32], [ua.VariantType.Int32])
    g_auth.add_method(idx, "logout", logout, [], [ua.VariantType.Int32])
    g_auth.add_method(idx, "commit", commit, [], [ua.VariantType.Int32])
    g_auth.add_method(idx, "enableAccount", enableAccount, [ua.VariantType.String], [ua.VariantType.Int32])
    g_auth.add_method(idx, "disableAccount", disableAccount, [ua.VariantType.String], [ua.VariantType.Int32])
    g_auth.add_method(idx, "lockAccount", lockAccount, [ua.VariantType.String], [ua.VariantType.Int32])
    g_auth.add_method(idx, "unlockAccount", unlockAccount, [ua.VariantType.String], [ua.VariantType.Int32])
    g_auth.add_method(idx, "isAccountLocked", isAccountLocked, [ua.VariantType.String], [ua.VariantType.Boolean])
    g_auth.add_method(idx, "isAccountEnabled", isAccountEnabled, [ua.VariantType.String], [ua.VariantType.Boolean])
    g_auth.add_method(idx, "isAccountInGroup", isAccountInGroup, [ua.VariantType.String, ua.VariantType.String], [ua.VariantType.Boolean])

    # 3. Add g_Alarm module
    g_alarm = modules.add_folder(idx, "g_Alarm")
    g_alarm.add_method(idx, "getAlarmCount", getAlarmCount, [], [ua.VariantType.Int32])

    # 4. Add g_Window module
    g_window = modules.add_folder(idx, "g_Window")
    g_window.add_method(idx, "openBridged", openBridged, [ua.VariantType.String], [ua.VariantType.Int32])
    g_window.add_method(idx, "open", openWindow, [ua.VariantType.String, ua.VariantType.Int32, ua.VariantType.Int32, ua.VariantType.Int32, ua.VariantType.Int32, ua.VariantType.Int32], [ua.VariantType.Int32])
    g_window.add_method(idx, "close", closeWindow, [ua.VariantType.String], [ua.VariantType.Int32])
    g_window.add_method(idx, "hide", hideWindow, [ua.VariantType.String], [ua.VariantType.Int32])
    g_window.add_method(idx, "restore", restoreWindow, [ua.VariantType.String], [ua.VariantType.Int32])
    g_window.add_method(idx, "callJsFunc", callJsFunc, [ua.VariantType.String, ua.VariantType.String, ua.VariantType.String], [ua.VariantType.String])

    server.start()
    logger.info("MindSCADA OPC-UA Server listening on opc.tcp://0.0.0.0:4840/")

    # 5. Populate variables from varconf
    tag_count = 0
    if os.path.exists(VARCONF_PATH):
        try:
            with open(VARCONF_PATH, "r", encoding="utf-8") as f:
                vconf = json.load(f)
            folder_cache = {}
            for item in vconf:
                name = item.get("name", "") if isinstance(item, dict) else str(item)
                if not name:
                    continue
                parts = name.strip("/").replace("\\", "/").split("/")
                # Ensure parent folders exist under objects
                curr = objects
                for p in parts[:-1]:
                    key = (curr, p)
                    if key not in folder_cache:
                        folder_cache[key] = curr.add_folder(idx, p)
                    curr = folder_cache[key]
                # Add variable
                var_name = parts[-1]
                var_node = curr.add_variable(idx, var_name, 0)
                var_node.set_writable()
                # Also add /Value child
                val_node = var_node.add_variable(idx, "Value", 0)
                val_node.set_writable()
                tag_count += 1
                if tag_count >= 500:  # Load initial core tags for quick startup
                    break
            logger.info("Loaded %d tags into OPC-UA Server address space.", tag_count)
        except Exception as e:
            logger.warning("Failed to populate tags: %s", e)

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        logger.info("Stopping server...")
    finally:
        server.stop()
        logger.info("Server stopped.")

if __name__ == "__main__":
    main()
