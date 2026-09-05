def can_build(env, platform):
    return platform in ["windows", "linuxbsd"]

def get_doc_classes():
    return ["QtWindowOverlay"]

def get_doc_path():
    return "doc_classes"

def configure(env):
    pass
