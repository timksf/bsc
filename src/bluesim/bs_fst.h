#ifndef __BS_FST_H__
#define __BS_FST_H__

/* The FST functionality declared in this header file
 * is internal, C++ functionality.  The external FST interface
 * is declared in the bluesim_kernel_api.h file.
 */
#include <cstdio>
#include <list>
#include <set>
#include <map>
#include "bluesim_types.h"
#include "bs_wide_data.h"
#include "bs_module.h"
#include "bs_vcd.h"  /* for tVCDDumpType */

struct fstWriterContext;  /* forward declaration from fstapi.h */

/* used by the kernel and schedule */

extern void fst_reset(tSimStateHdl simHdl);
extern void fst_dump_xs(tSimStateHdl simHdl);
extern bool fst_set_state(tSimStateHdl simHdl, bool enabled);
extern bool fst_is_active(tSimStateHdl simHdl);
extern void fst_keep_ids(tSimStateHdl simHdl);
extern bool fst_write_header(tSimStateHdl simHdl);
extern tVCDDumpType get_FST_dump_type(tSimStateHdl simHdl);
extern bool fst_check_file_size(tSimStateHdl simHdl);
extern void fst_set_backing_instance(tSimStateHdl simHdl, bool b);

/* used by modules */
extern tUInt32 fst_depth(tSimStateHdl simHdl);
extern bool fst_is_backing_instance(tSimStateHdl simHdl);
extern unsigned int fst_reserve_ids(tSimStateHdl simHdl, unsigned int num);
extern void fst_add_clock_def(tSimStateHdl simHdl,
			      Module* module, const char* s, unsigned int num);
extern void fst_set_clock(tSimStateHdl simHdl,
			  unsigned int num, tClock handle);
extern void fst_write_scope_start(tSimStateHdl simHdl, const char* name);
extern void fst_write_scope_end(tSimStateHdl simHdl);
extern void fst_write_def(tSimStateHdl simHdl,
			  unsigned int num,
			  const char* name,
			  unsigned int width);
extern void fst_advance(tSimStateHdl simHdl, bool immediate);
extern void fst_write_x(tSimStateHdl simHdl,
			unsigned int num,
			unsigned int width);
extern void fst_write_val(tSimStateHdl simHdl,
			  unsigned int num,
			  tClockValue value,
			  unsigned int width);
extern void fst_write_val(tSimStateHdl simHdl,
			  unsigned int num,
			  bool value,
			  unsigned int width);
extern void fst_write_val(tSimStateHdl simHdl,
			  unsigned int num,
			  tUInt8 value,
			  unsigned int width);
extern void fst_write_val(tSimStateHdl simHdl,
			  unsigned int num,
			  tUInt32 value,
			  unsigned int width);
extern void fst_write_val(tSimStateHdl simHdl,
			  unsigned int num,
			  tUInt64 value,
			  unsigned int width);
extern void fst_write_val(tSimStateHdl simHdl,
			  unsigned int num,
			  const tUWide& value,
			  unsigned int width);

/* Array/memory dumping support (FST only) */
extern bool fst_trace_memories(tSimStateHdl simHdl);
extern tUInt32 fst_max_array_depth(tSimStateHdl simHdl);
extern bool fst_use_array_scope(tSimStateHdl simHdl);
extern void fst_write_array_scope_start(tSimStateHdl simHdl, const char* name);
extern void fst_write_array_scope_end(tSimStateHdl simHdl);
extern void fst_write_array_attr(tSimStateHdl simHdl, unsigned int num_elements);

#endif /* __BS_FST_H__ */
