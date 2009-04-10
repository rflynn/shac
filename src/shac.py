#!/usr/bin/python
# ex: set ff=dos ts=2 et:

import os

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

class File:
  pass

class Path:
  pass

class Group:
  pass

class User:
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

class Reason:
  pass


print "cwd = %s" % os.getcwd()
print "uid = %d" % os.getuid()



