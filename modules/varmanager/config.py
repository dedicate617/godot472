# config.py

def can_build(env, platform):
    return (platform == "server" or platform == "windows" or platform == "linuxbsd" or platform == "web" or platform == "javascript")

def configure(env):
    pass
