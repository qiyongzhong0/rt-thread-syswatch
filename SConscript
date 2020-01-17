from building import *

cwd = GetCurrentDir()
path = [cwd+'/inc']
src  = Glob('src/*.c')
 
group = DefineGroup('syswatch', src, depend = ['PKG_USING_SYSWATCH'], CPPPATH = path)

Return('group')