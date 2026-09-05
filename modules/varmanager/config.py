# config.py

def can_build(env, platform):
    # return True
    # return (platform == "x11" or platform == "server" or platform == "windows" or platform == "osx")
    return (platform == "server" or platform == "windows" or platform == "linuxbsd")

def configure(env):
    pass
