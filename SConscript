from building import *

cwd = GetCurrentDir()
path = [cwd]
src  = Glob('*.c')
 
group = DefineGroup('syswatch', src, depend = [''], CPPPATH = path)

Return('group')