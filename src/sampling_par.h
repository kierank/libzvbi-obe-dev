#ifndef __SAMPLING_PAR_H__
#define __SAMPLING_PAR_H__

#include "decoder.h"

/* Public */

typedef vbi_raw_decoder vbi_sampling_par;

#define VBI_VIDEOSTD_SET_EMPTY 0
#define VBI_VIDEOSTD_SET_PAL_BG 1
#define VBI_VIDEOSTD_SET_625_50 1
#define VBI_VIDEOSTD_SET_525_60 2
#define VBI_VIDEOSTD_SET_ALL 3
typedef uint64_t vbi_videostd_set;

/* Private */

extern vbi_service_set
vbi_sampling_par_from_services (vbi_sampling_par *    sp,
				unsigned int *         max_rate,
				vbi_videostd_set      videostd_set,
				vbi_service_set       services);
extern vbi_service_set
vbi_sampling_par_check_services
                                (const vbi_sampling_par *sp,
                                 vbi_service_set       services,
                                 unsigned int           strict)
  __attribute__ ((_vbi_pure));

extern vbi_videostd_set
_vbi_videostd_set_from_scanning	(int			scanning);

extern vbi_service_set
_vbi_sampling_par_from_services_log
                                (vbi_sampling_par *    sp,
                                 unsigned int *         max_rate,
                                 vbi_videostd_set      videostd_set,
                                 vbi_service_set       services,
                                 _vbi_log_hook *       log);
extern vbi_service_set
_vbi_sampling_par_check_services_log
                                (const vbi_sampling_par *sp,
                                 vbi_service_set       services,
                                 unsigned int           strict,
                                 _vbi_log_hook *       log)
  __attribute__ ((_vbi_pure));
extern vbi_bool
_vbi_sampling_par_valid_log    (const vbi_sampling_par *sp,
                                 _vbi_log_hook *       log)
  __attribute__ ((_vbi_pure));

#endif /* __SAMPLING_PAR_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
