#include "bluesim_kernel_api.h"

#include <string>

void dollar_dumpfile(tSimStateHdl simHdl)
{
  bk_set_VCD_file(simHdl, "dump.vcd");
}

void dollar_dumpfile(tSimStateHdl simHdl,
		     const char* /* unused */, const std::string* name)
{
  bk_set_VCD_file(simHdl, name->c_str());
}

void dollar_dumpvars(tSimStateHdl simHdl,
		     const char* /* unused */, unsigned int depth)
{
  bk_set_VCD_depth(simHdl, depth);
  bk_enable_VCD_dumping(simHdl);
}

void dollar_dumpon(tSimStateHdl simHdl)
{
  bk_enable_VCD_dumping(simHdl);
}

void dollar_dumpoff(tSimStateHdl simHdl)
{
  bk_disable_VCD_dumping(simHdl);
}

void dollar_dumpall(tSimStateHdl simHdl)
{
  bk_VCD_checkpoint(simHdl);
}

void dollar_dumplimit(tSimStateHdl simHdl,
		      const char* /* unused */, unsigned long long bytes)
{
  bk_set_VCD_filesize_limit(simHdl, bytes);
}

void dollar_dumpflush(tSimStateHdl simHdl)
{
  bk_flush_VCD_output(simHdl);
}

/* FST equivalents of the $dump* system tasks */

void dollar_fstdumpfile(tSimStateHdl simHdl)
{
  bk_set_FST_file(simHdl, "dump.fst");
}

void dollar_fstdumpfile(tSimStateHdl simHdl,
		        const char* /* unused */, const std::string* name)
{
  bk_set_FST_file(simHdl, name->c_str());
}

void dollar_fstdumpvars(tSimStateHdl simHdl,
		        const char* /* unused */, unsigned int depth)
{
  bk_set_FST_depth(simHdl, depth);
  bk_enable_FST_dumping(simHdl);
}

void dollar_fstdumpon(tSimStateHdl simHdl)
{
  bk_enable_FST_dumping(simHdl);
}

void dollar_fstdumpoff(tSimStateHdl simHdl)
{
  bk_disable_FST_dumping(simHdl);
}

void dollar_fstdumpall(tSimStateHdl simHdl)
{
  bk_FST_checkpoint(simHdl);
}

void dollar_fstdumplimit(tSimStateHdl simHdl,
		         const char* /* unused */, unsigned long long bytes)
{
  bk_set_FST_filesize_limit(simHdl, bytes);
}

void dollar_fstdumpflush(tSimStateHdl simHdl)
{
  bk_flush_FST_output(simHdl);
}
