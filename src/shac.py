#!/usr/bin/python
# ex: set ff=dos ts=2 et:

import os

# 
# 
# 
class Perm:

  NONE = 0x00
  Read = 0x01
  Writ = 0x02
  Exec = 0x04
  Crea = 0x08
  Dele = 0x10

  def fromstr(self, s):
    p = Perm.NONE
    for c in s.lower():
      if "r" == c: p |= Perm.Read
      if "w" == c: p |= Perm.Writ
      if "x" == c: p |= Perm.Exec
      if "c" == c: p |= Perm.Crea
      if "d" == c: p |= Perm.Dele
    return p

  def tostr(self, p):
    s = ""
    if p & Perm.Read: s += "r"
    if p & Perm.Writ: s += "w"
    if p & Perm.Exec: s += "x"
    if p & Perm.Crea: s += "c"
    if p & Perm.Dele: s += "d"
    return s

# test Perm.fromstr
assert Perm().fromstr("")      == Perm.NONE
assert Perm().fromstr(".")     == Perm.NONE
assert Perm().fromstr("r")     == Perm.Read
assert Perm().fromstr("w")     == Perm.Writ
assert Perm().fromstr("x")     == Perm.Exec
assert Perm().fromstr("c")     == Perm.Crea
assert Perm().fromstr("d")     == Perm.Dele 
assert Perm().fromstr("rwxcd") == Perm.Read | Perm.Writ | Perm.Exec | Perm.Crea | Perm.Dele

# test Perm.tostr
assert ""      == Perm().tostr(Perm.NONE)
assert "r"     == Perm().tostr(Perm.Read)
assert "w"     == Perm().tostr(Perm.Writ)
assert "x"     == Perm().tostr(Perm.Exec)
assert "c"     == Perm().tostr(Perm.Crea)
assert "d"     == Perm().tostr(Perm.Dele)
assert "r"     == Perm().tostr(Perm.Read)
assert "rwxcd" == Perm().tostr(Perm.Read | Perm.Writ | Perm.Exec | Perm.Crea | Perm.Dele)

# 
# 
# 
class Reason:

  path  = ""    # 
  label = 0     # ???
  yes   = [ ]   # reasons why we CAN access file/dir
  no    = [ ]   # reasons why we CAN'T

  # reasons why we don't have access
  class No:
    Unknown       = 0
    Read          = 1
    Write         = 2
    Exec          = 3
    Create        = 4
    Delete        = 5
    MntPtReadOnly = 6
    MntPtNoExec   = 7
    Sticky        = 8
    Dependency    = 9
    Str_ = [
      "Unknown",
      "NoRead",
      "NoWrite",
      "NoExec",
      "NoCreate",
      "NoDelete",
      "MntPtReadOnly",
      "MntPtNoExec",
      "Sticky",
      "Dependency"
      ]
    
    def toStr(self, r):
      return self.Str_[r]

  class Yes:
    # reasons why we do have access
    UserRead   =  0
    UserWrite  =  1
    UserExec   =  2
    GroupRead  =  3
    GroupWrite =  4
    GroupExec  =  5
    OtherRead  =  6
    OtherWrite =  7
    OtherExec  =  8
    Sticky     =  9
    Owner      = 10
    Root       = 11
    Str_ = [
      "UserRead",
      "UserWrite",
      "UserExec",
      "GroupRead",
      "GroupWrite",
      "GroupExec",
      "OtherRead",
      "OtherWrite",
      "OtherExec",
      "Sticky",
      "Owner",
      "Root"
      ]
    def toStr(self, r):
      return self.Str_[r]
  
assert "Dependency" == Reason().No().toStr(Reason.No.Dependency)
assert "Root"       == Reason().Yes().toStr(Reason.Yes.Root)

class File:
  pass


class Group:
  NONE = -1

class User:
  NONE = -1
  name = ""
  uid = 0
  groups = [ ]
  def __init__(self, name):
    self.name = name

class MountPoint:
  dev = ""
  dir = ""

  def __init__(self):
    return
  
  def isReadOnly(self):
    return True

  def isNoExec(self):
    return True


class Path:
  abspath   = ""
  dir       = ""
  component = ""
  symlink   = ""          # path this file points to if symlink
  uid       = User.NONE
  gid       = Group.NONE
  status    = Perm()
  mode      = 0 # ???
  mntpt     = MountPoint()

  def isSymlink(self):
    pass

  def isDir(self):
    pass

  def isFile(self):
    pass

  def isSticky(self):
    pass

  def isMountPoint(self):
    pass

  def StatusNotOK(self):
    pass


print "cwd = %s" % os.getcwd()
print "uid = %d" % os.getuid()


