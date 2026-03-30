#include <time.h>
#include <cstdio>
#include <cstring>
#include <string>

#include "bluesim_kernel_api.h"
#include "kernel.h"
#include "bs_fst.h"
#include "fstapi.h"

/* FST change buffer mechanism
 *
 * This mirrors the VCD change buffer mechanism.
 * Bluesim computes all changes at the posedge, according to TRS
 * semantics, but the expected output (to match Verilog) requires
 * that the combinational logic values take on their new values
 * after the previous posedge.
 *
 * Changes are buffered and accumulated until all clocks which
 * can assign a change to that time have occurred, at which point
 * changes for that time are emitted to the FST file.
 */

tStatus bk_set_FST_file(tSimStateHdl simHdl, const char* name)
{
  tFSTState* s = &(simHdl->fst);

  if (s->fst_ctx != NULL)
    fstWriterClose(s->fst_ctx);
  s->fst_file_name.resize(0);

  s->state = VCD_OFF;
  if (name == NULL)
  {
    s->fst_ctx = NULL;
    return BK_SUCCESS;
  }

  s->fst_file_name = name;
  s->fst_ctx = fstWriterCreate(name, 1 /* use compressed hierarchy */);

  if (s->fst_ctx == NULL)
  {
    s->fst_file_name.resize(0);
    perror(name);
    return BK_ERROR;
  }

  fstWriterSetFileType(s->fst_ctx, FST_FT_VERILOG);
  fstWriterSetPackType(s->fst_ctx, FST_WR_PT_LZ4);

  return BK_SUCCESS;
}

const char* bk_get_FST_file_name(tSimStateHdl simHdl)
{
  return (simHdl->fst).fst_file_name.c_str();
}

void bk_set_FST_depth(tSimStateHdl simHdl, tUInt32 depth)
{
  if ((simHdl->fst).state == VCD_OFF)
    (simHdl->fst).fst_depth = depth;
}

tStatus bk_FST_checkpoint(tSimStateHdl simHdl)
{
  if ((simHdl->fst).fst_ctx == NULL)
  {
    if (bk_set_FST_file(simHdl, "dump.fst") != BK_SUCCESS)
      return BK_ERROR;
  }

  (simHdl->fst).fst_checkpoint = true;

  return BK_SUCCESS;
}

void bk_set_FST_filesize_limit(tSimStateHdl simHdl, tUInt64 bytes)
{
  tFSTState* s = &(simHdl->fst);
  s->fst_filesize_limit = bytes;
  if (s->fst_ctx != NULL)
    fstWriterSetDumpSizeLimit(s->fst_ctx, bytes);
}

void bk_flush_FST_output(tSimStateHdl simHdl)
{
  if ((simHdl->fst).fst_ctx != NULL)
    fstWriterFlushContext((simHdl->fst).fst_ctx);
}

/* FST routines used by the simulation kernel */

// forward declarations
static void flush_fst_changes(tSimStateHdl simHdl);
static void emit_fst_X(tSimStateHdl simHdl, unsigned int bits, unsigned int num);
static void emit_fst_change(tSimStateHdl simHdl,
			    unsigned int bits, unsigned int num, tUInt64 val);
static void emit_fst_change(tSimStateHdl simHdl,
			    unsigned int bits, unsigned int num, const tUWide& val);

void fst_reset(tSimStateHdl simHdl)
{
  tFSTState* s = &(simHdl->fst);

  s->changes_now = false;
  s->min_pending = bk_now(simHdl);
  flush_fst_changes(simHdl);
  if (s->fst_ctx != NULL)
    fstWriterClose(s->fst_ctx);
  s->fst_ctx = NULL;
  if (! s->fst_file_name.empty())
    s->fst_file_name.resize(0);
  s->state = VCD_OFF;
  s->fst_enabled = false;
  s->fst_depth = 0;
  s->fst_filesize_limit = 0llu;
  s->fst_checkpoint = false;
  s->go_xs = false;
  s->next_seq_num = 0;
  s->kept_seq_num = 0;
  s->is_backing_instance = false;
  s->clk_map.clear();
  s->changes.clear();
  s->last_time_written = ~bk_now(simHdl);
  s->handle_map.clear();
}

void fst_dump_xs(tSimStateHdl simHdl)
{
  (simHdl->fst).go_xs = true;
}

bool fst_set_state(tSimStateHdl simHdl, bool enabled)
{
  if (enabled && ((simHdl->fst).fst_ctx == NULL))
  {
    if (bk_set_FST_file(simHdl, "dump.fst") != BK_SUCCESS)
      return false;
  }

  (simHdl->fst).fst_enabled = enabled;

  return true;
}

bool fst_is_active(tSimStateHdl simHdl)
{
  return ((simHdl->fst).fst_enabled ||
	  (simHdl->fst).fst_checkpoint ||
	  (simHdl->fst).go_xs);
}

tVCDDumpType get_FST_dump_type(tSimStateHdl simHdl)
{
  tVCDDumpType ret;
  tFSTState* s = &(simHdl->fst);

  if (s->fst_checkpoint)
  {
    ret = VCD_DUMP_CHECKPOINT;
    s->fst_checkpoint = false;
    s->go_xs = ! s->fst_enabled;
  }
  else if (s->go_xs)
  {
    ret = VCD_DUMP_XS;
    s->go_xs = false;
    s->state = VCD_DISABLED;
  }
  else
  {
    switch (s->state)
    {
      case VCD_HEADER:   { ret = VCD_DUMP_INITIAL; break; }
      case VCD_ENABLED:  { ret = VCD_DUMP_CHANGES; break; }
      case VCD_DISABLED: { ret = VCD_DUMP_RESTART; break; }
      default:           { ret = VCD_DUMP_NONE; break; }
    }
    s->state = VCD_ENABLED;
  }

  return ret;
}

bool fst_write_header(tSimStateHdl simHdl)
{
  tFSTState* s = &(simHdl->fst);

  if (s->fst_ctx == NULL)
    return false;

  if (s->state != VCD_OFF)
    return false;

  s->state = VCD_HEADER;

  time_t t = time(NULL);
  fstWriterSetDate(s->fst_ctx, ctime(&t));
  fstWriterSetVersion(s->fst_ctx, "Bluespec FST dumper 1.0");
  fstWriterSetTimescaleFromString(s->fst_ctx, s->fst_timescale);

  s->next_seq_num = s->kept_seq_num;

  return true;
}

bool fst_check_file_size(tSimStateHdl simHdl)
{
  tFSTState* s = &(simHdl->fst);
  if (s->fst_ctx != NULL && fstWriterGetDumpSizeLimitReached(s->fst_ctx))
  {
    fst_reset(simHdl);
    return false;
  }
  return true;
}

void fst_set_backing_instance(tSimStateHdl simHdl, bool b)
{
  (simHdl->fst).is_backing_instance = b;
}


/* FST routines called from modules */

tUInt32 fst_depth(tSimStateHdl simHdl)
{
  return (simHdl->fst).fst_depth;
}

bool fst_is_backing_instance(tSimStateHdl simHdl)
{
  return (simHdl->fst).is_backing_instance;
}

unsigned int fst_reserve_ids(tSimStateHdl simHdl, unsigned int num)
{
  unsigned int n = (simHdl->fst).next_seq_num;
  (simHdl->fst).next_seq_num += num;
  return n;
}

void fst_keep_ids(tSimStateHdl simHdl)
{
  (simHdl->fst).kept_seq_num = (simHdl->fst).next_seq_num;
}

void fst_add_clock_def(tSimStateHdl simHdl,
		       Module* module, const char* s, unsigned int num)
{
  /* For FST, clock defs are created via fst_write_def during hierarchy setup. */
}

void fst_set_clock(tSimStateHdl simHdl, unsigned int num, tClock handle)
{
  if (handle == BAD_CLOCK_HANDLE)
    return;

  (simHdl->fst).clk_map.insert(std::make_pair(num, handle));
}

void fst_write_scope_start(tSimStateHdl simHdl, const char* name)
{
  tFSTState* s = &(simHdl->fst);
  if (s->fst_ctx != NULL)
    fstWriterSetScope(s->fst_ctx, FST_ST_VCD_MODULE, name, NULL);
}

void fst_write_scope_end(tSimStateHdl simHdl)
{
  tFSTState* s = &(simHdl->fst);
  if (s->fst_ctx != NULL)
    fstWriterSetUpscope(s->fst_ctx);
}

void fst_write_def(tSimStateHdl simHdl,
		   unsigned int num,
		   const char* name,
		   unsigned int width)
{
  tFSTState* s = &(simHdl->fst);
  if (s->fst_ctx == NULL) return;

  /* Check if this num was already mapped (aliasing) */
  std::map<unsigned int, fstHandle>::iterator it = s->handle_map.find(num);
  fstHandle alias = 0;
  if (it != s->handle_map.end())
    alias = it->second;

  fstHandle h = fstWriterCreateVar(s->fst_ctx,
				   FST_VT_VCD_REG,
				   FST_VD_IMPLICIT,
				   width,
				   name,
				   alias);
  if (alias == 0)
    s->handle_map[num] = h;
}

void fst_advance(tSimStateHdl simHdl, bool immediate)
{
  /* update the min_pending value */
  (simHdl->fst).min_pending = bk_now(simHdl);
  for (tClock clk = 0; clk < bk_num_clocks(simHdl); ++clk)
  {
    tTime le = bk_clock_combinational_time(simHdl, clk);
    if (le < (simHdl->fst).min_pending)
      (simHdl->fst).min_pending = le;
  }

  /* write all changes prior to min_pending */
  flush_fst_changes(simHdl);

  /* update changes_now setting */
  (simHdl->fst).changes_now = immediate;
}

static void fst_output_at_time(tSimStateHdl simHdl, tTime time)
{
  if ((simHdl->fst).last_time_written == time)
    return;

  (simHdl->fst).last_time_written = time;

  fstWriterEmitTimeChange((simHdl->fst).fst_ctx, time);
}

static void flush_fst_changes(tSimStateHdl simHdl)
{
  for (std::map<tTime,tChangeList>::iterator cl = (simHdl->fst).changes.begin();
       cl != (simHdl->fst).changes.end();
       )
  {
    tTime t = cl->first;
    if (t >= (simHdl->fst).min_pending)
      return;

    fst_output_at_time(simHdl, t);

    /* Emit all FST entries for this time */
    tChangeList& ch_list = cl->second;
    for (tChangeList::iterator p = ch_list.begin();
         p != ch_list.end();
         ++p)
    {
      Change& ch = *p;
      if (ch.isX)
        emit_fst_X(simHdl, ch.bits, ch.num);
      else if (ch.bits <= 64)
        emit_fst_change(simHdl, ch.bits, ch.num, ch.narrow);
      else
        emit_fst_change(simHdl, ch.bits, ch.num, ch.wide);
    }

    (simHdl->fst).changes.erase(cl++);
  }
}

static tTime fst_time_of_change(tSimStateHdl simHdl, unsigned int num)
{
  std::pair<tClockMap::iterator,tClockMap::iterator> p
    = (simHdl->fst).clk_map.equal_range(num);

  // if we are in an immediate update situation, use the current time
  if ((simHdl->fst).changes_now)
    return bk_now(simHdl);

  // if there is no associated clock, use the current time
  if (p.first == p.second)
    return bk_now(simHdl);

  // otherwise, find the most recent combinational time of
  // any associated clock
  tTime recent = 0llu;
  tClockMap::iterator iter = p.first;
  while (iter != p.second)
  {
    tClock handle = iter->second;
    tTime le = bk_clock_combinational_time(simHdl, handle);
    if (le > recent)
      recent = le;
    ++iter;
  }

  return recent;
}

/* Helper to get the fstHandle for a given seq num */
static fstHandle fst_handle_for(tSimStateHdl simHdl, unsigned int num)
{
  std::map<unsigned int, fstHandle>::iterator it = (simHdl->fst).handle_map.find(num);
  if (it != (simHdl->fst).handle_map.end())
    return it->second;
  return 0;
}

/* Format a value as a binary string for FST emission */
static void format_binary_str(char* buf, unsigned int bits, tUInt64 value)
{
  for (unsigned int i = 0; i < bits; ++i)
    buf[i] = ((value >> (bits - 1 - i)) & 1) ? '1' : '0';
  buf[bits] = '\0';
}

void fst_write_x(tSimStateHdl simHdl,
		 unsigned int num,
		 unsigned int width)
{
  tTime time = fst_time_of_change(simHdl, num);
  if (time > (simHdl->fst).min_pending)
    (simHdl->fst).changes[time].push_back(Change(num, width));
  else
  {
    fst_output_at_time(simHdl, time);
    emit_fst_X(simHdl, width, num);
  }
}

void fst_write_val(tSimStateHdl simHdl,
		   unsigned int num,
		   tClockValue value,
		   unsigned int /* unused */)
{
  tTime time = fst_time_of_change(simHdl, num);
  tUInt64 val = (value == CLK_HIGH) ? 1 : 0;
  if (time > (simHdl->fst).min_pending)
    (simHdl->fst).changes[time].push_back(Change(num, 1, val));
  else
  {
    fst_output_at_time(simHdl, time);
    emit_fst_change(simHdl, 1, num, val);
  }
}

void fst_write_val(tSimStateHdl simHdl,
		   unsigned int num,
		   bool value,
		   unsigned int /* unused */)
{
  tTime time = fst_time_of_change(simHdl, num);
  tUInt64 val = value ? 1 : 0;
  if (time > (simHdl->fst).min_pending)
    (simHdl->fst).changes[time].push_back(Change(num, 1, val));
  else
  {
    fst_output_at_time(simHdl, time);
    emit_fst_change(simHdl, 1, num, val);
  }
}

void fst_write_val(tSimStateHdl simHdl,
		   unsigned int num,
		   tUInt8 value,
		   unsigned int width)
{
  tTime time = fst_time_of_change(simHdl, num);
  if (time > (simHdl->fst).min_pending)
    (simHdl->fst).changes[time].push_back(Change(num, width, (tUInt64)value));
  else
  {
    fst_output_at_time(simHdl, time);
    emit_fst_change(simHdl, width, num, (tUInt64)value);
  }
}

void fst_write_val(tSimStateHdl simHdl,
		   unsigned int num,
		   tUInt32 value,
		   unsigned int width)
{
  tTime time = fst_time_of_change(simHdl, num);
  if (time > (simHdl->fst).min_pending)
    (simHdl->fst).changes[time].push_back(Change(num, width, (tUInt64)value));
  else
  {
    fst_output_at_time(simHdl, time);
    emit_fst_change(simHdl, width, num, (tUInt64)value);
  }
}

void fst_write_val(tSimStateHdl simHdl,
		   unsigned int num,
		   tUInt64 value,
		   unsigned int width)
{
  tTime time = fst_time_of_change(simHdl, num);
  if (time > (simHdl->fst).min_pending)
    (simHdl->fst).changes[time].push_back(Change(num, width, value));
  else
  {
    fst_output_at_time(simHdl, time);
    emit_fst_change(simHdl, width, num, value);
  }
}

void fst_write_val(tSimStateHdl simHdl,
		   unsigned int num,
		   const tUWide& value,
		   unsigned int width)
{
  tTime time = fst_time_of_change(simHdl, num);
  if (time > (simHdl->fst).min_pending)
    (simHdl->fst).changes[time].push_back(Change(num, value));
  else
  {
    fst_output_at_time(simHdl, time);
    emit_fst_change(simHdl, width, num, value);
  }
}


/* Emit an X value change to the FST file */
static void emit_fst_X(tSimStateHdl simHdl, unsigned int bits, unsigned int num)
{
  fstHandle h = fst_handle_for(simHdl, num);
  if (h == 0) return;

  char buf[256];
  char* p = buf;
  bool need_free = false;
  if (bits + 1 > sizeof(buf))
  {
    p = (char*)malloc(bits + 1);
    need_free = true;
  }
  for (unsigned int i = 0; i < bits; ++i)
    p[i] = 'x';
  p[bits] = '\0';

  fstWriterEmitValueChange((simHdl->fst).fst_ctx, h, p);

  if (need_free)(free(p));
}

/* Emit a narrow (<=64 bit) value change */
static void emit_fst_change(tSimStateHdl simHdl,
			    unsigned int bits, unsigned int num, tUInt64 val)
{
  fstHandle h = fst_handle_for(simHdl, num);
  if (h == 0) return;

  char buf[65];
  format_binary_str(buf, bits, val);
  fstWriterEmitValueChange((simHdl->fst).fst_ctx, h, buf);
}

/* Emit a wide (>64 bit) value change */
static void emit_fst_change(tSimStateHdl simHdl,
			    unsigned int bits, unsigned int num, const tUWide& val)
{
  fstHandle h = fst_handle_for(simHdl, num);
  if (h == 0) return;

  char buf_stack[256];
  char* buf = buf_stack;
  bool need_free = false;
  if (bits + 1 > sizeof(buf_stack))
  {
    buf = (char*)malloc(bits + 1);
    need_free = true;
  }

  /* Convert wide value to binary string, MSB first */
  for (unsigned int i = 0; i < bits; ++i)
  {
    unsigned int bit_pos = bits - 1 - i;
    unsigned int word_idx = bit_pos / 32;
    unsigned int bit_idx = bit_pos % 32;
    buf[i] = (word_idx < val.numWords() && ((val[word_idx] >> bit_idx) & 1))
             ? '1' : '0';
  }
  buf[bits] = '\0';

  fstWriterEmitValueChange((simHdl->fst).fst_ctx, h, buf);

  if (need_free) free(buf);
}
