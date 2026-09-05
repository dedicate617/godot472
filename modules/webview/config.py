def can_build(env, platform):
    return (platform == "windows" or platform == "linuxbsd")

def get_doc_classes():
    return [
        "WebViewOverlay"
    ]

def get_doc_path():
    return "doc_classes"

def configure(env):
    pass
