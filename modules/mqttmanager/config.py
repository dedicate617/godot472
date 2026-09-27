# config.py

def can_build(env, platform):
    # return env.editor_build
    return (platform == "server" or platform == "windows" or platform == "linuxbsd" or platform == "web" or platform == "javascript")

def configure(env):
    pass
