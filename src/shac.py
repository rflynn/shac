#!/usr/bin/python
# ex: set ff=dos ai ts=2 et:

import os
import popen2
import string
import re
import stat

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

  def __init__(self):
    self.path  = ""    # 
    self.label = 0     # ???
    self.yes   = [ ]   # reasons why we CAN access file/dir
    self.no    = [ ]   # reasons why we CAN'T

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
  def __init__(self, name):
    self.name     = name
    self.uid      = 0
    self.groups   = [ ]

class MountPoint:
  # str is a line of output from 'mount' in the format:
  #   /dev/sda3 on / type ext3 (rw,errors=remount-ro)
  def __init__(self, str):
    self.dev        = ""
    self.path       = ""
    self.filesystem = ""
    self.params     = [ ]
    self.isreadonly = False
    self.isnoexec   = False
    self.dev,_,self.path,_,self.filesystem,params = str.split()
    if len(params) > 1:
      params = params[1:-1].split(",")
      for p in params:
        x = p.split("=")
        if len(x) == 1:
          x.append(x[0])
        assert 2 == len(x)
        self.params.append(tuple(x))
      self.processParams()

  def dump(self):
    print "dev=%s path=%s filesystem=%s param=%s" % \
      (self.dev, self.path, self.filesystem, self.params)

  def processParams(self):
    for p in self.params:
      k,v = p
      if "rw" == k:
        self.isreadonly = False
      elif "ro" == k:
        self.isreadonly = True
      elif "noexec" == k:
        self.isnoexec = True
      print "%s=%s" % (k,v)

# whoops, python doesn't have proper static methods; bummer!
# <URL: http://code.activestate.com/recipes/52304/>
# This is easy to solve with a simple tiny wrapper:
class Callable:
  def __init__(self, anycallable):
    self.__call__ = anycallable

class Path:
  cwd = os.getcwd()
  def init(self, path):
    self.origpath  = path         # 
    self.abspath   = ""           # 
    self.dir       = ""           # 
    self.component = ""           # 
    self.symlink   = ""           # path this file points to if symlink
    self.uid       = User.NONE    # 
    self.gid       = Group.NONE   # 
    self.status    = Perm()       # 
    self.mode      = 0            # ???
    self.mntpt     = None         # 
    self.issymlink = False        # 
    self.isdir     = False        # 
    self.isfile    = False        # 
    self.issticky  = False        # 
    self.ismntpt   = False        # 

  def __init__(self, path):
    self.init(path)

  # normalize the given path, which may be relative, to an absolute path
  def normalize(path, cwd):
    normal = ""
    if len(path) > 0:
      pieces = path.split("/")
      # remove empty components, i.e. "//" -> "/"
      pieces = filter(lambda x: "" != x, pieces)
      # remove self-referential "." components
      pieces = filter(lambda x: "." != x, pieces)
      # interpolate ".."
      i = 0
      while i < len(pieces):
        if ".." != pieces[i]:
          i = i + 1
        else:
          pieces.remove(pieces[i])
          if i > 0:
            pieces.remove(pieces[i-1])
      # all done, re-join
      normal = "/".join(pieces)
      if "/" == path[0:1]:
        # path is absolute
        normal = "/" + normal
      else:
        # path is relative, use cwd
        if len(cwd) > 0 and "/" != cwd[-1]:
          normal = "/" + normal
        normal = cwd + normal
    return normal
  # sneaky trick to make normalize a static-like method
  normalize = Callable(normalize)

  # calculate property values
  def calc(self):
    try:
      st = os.lstat("/")
    except OSError:
      err
    mode = st[stat.ST_MODE]
    uid  = st[stat.ST_UID]
    gid  = st[stat.ST_GID]

  def StatusNotOK(self):
    pass

print "cwd = %s" % os.getcwd()
print "uid = %d" % os.getuid()

# build dictionary of all system mountpoints
MountPoints = { }
fin, fout = popen2.popen2("mount")
for line in fin:
  m = MountPoint(line.strip())
  m.dump()
  print "m.path=%s" % m.path
  MountPoints[m.path] = m
fin.close()

# path normalization unit test
PathTest = [
  # handle empty
  ("",        ""),
  # merge duplicate path separators
  ("/",       "/"),
  ("//",      "/"),
  ("///",     "/"),
  # remove "."
  (".",       ""),
  ("/.",      "/"),
  ("/./",     "/"),
  ("/./.",    "/"),
  # handle ".."
  ("..",      ""),
  ("/..",     "/"),
  ("/../..",  "/"),
  ("/./..",   "/"),
  ("/../.",   "/"),
  # handle spaces
  ("/ /",     "/ "),
  # various
  ("/a/b/c",  "/a/b/c"),
  ("/a/b/c/", "/a/b/c"),
  ("/a/./b",  "/a/b"),
  ("/a/../b", "/b"),
  ("/../b",   "/b"),
  # account for cwd
]
for pt in PathTest:
  before,after = pt
  actual = Path.normalize(before, "")
  print "checking %-7s -> %-7s (%s)..." % (before, after, actual)
  assert after == actual

st   = os.lstat("/")
mode = st[stat.ST_MODE]
uid  = st[stat.ST_UID]
gid  = st[stat.ST_GID]

print "mode=%08x uid=%d gid=%d" % (mode,uid,gid)

