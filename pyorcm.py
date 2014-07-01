import os, ctypes
from ctypes import *

STRING = c_char_p
int8_t = c_int8
orcm_node_state_t = int8_t

class liborcm_node_t (Structure):
    _fields_ = [
                ('name', STRING),
                ('state', orcm_node_state_t),
               ]

__all__ = ['orcm_node_state_t', 'liborcm_node_t', 'int8_t']

if "posix" in os.name:
    orcm = CDLL("liborcmscd.so", mode=ctypes.RTLD_GLOBAL)
else:
    orcm = CDLL("liborcmscd.so")

P_node_t = POINTER(liborcm_node_t)
PP_node_t = POINTER(P_node_t)
orcm.get_nodes.argtypes = [POINTER(PP_node_t), POINTER(c_int)]
orcm.get_nodes.restype = c_int
nodelist = PP_node_t()
node_count = c_int(0)

ORCM_NODE_STATE_UNDEF   = 0
ORCM_NODE_STATE_UNKNOWN = 1
ORCM_NODE_STATE_UP      = 2
ORCM_NODE_STATE_DOWN    = 3
ORCM_NODE_STATE_SESTERM = 4

all_nodes = set()
down_nodes = set()

orcm.get_nodes(byref(nodelist), byref(node_count))
for i in range(node_count.value):
    name = nodelist[i].contents.name
    state = nodelist[i].contents.state
    all_nodes.add(name)
    if (state == ORCM_NODE_STATE_UNKNOWN) or (state == ORCM_NODE_STATE_DOWN):
        down_nodes.add(name)

print "ALL NODES:"
print ', '.join(all_nodes)
print "DOWN NODES:"
print ', '.join(down_nodes)
