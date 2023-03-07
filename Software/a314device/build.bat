vc -DMODEL_TD=1 romtag.asm driver_task.c debug.c device.c startup.c sockets.c pi_if_td.c int_server_td.asm fix_mem_region.c cmem.c check_a314_mapping.asm -O3 -nostdlib -o a314-td.device
vc -DMODEL_CP=1 romtag.asm driver_task.c debug.c device.c startup.c sockets.c pi_if_cp.c int_server_cp.asm memory_allocator.c -O3 -nostdlib -o a314-cp.device
