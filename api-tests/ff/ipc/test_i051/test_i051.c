/** @file
 * Copyright (c) 2019-2020, Arm Limited or its affiliates. All rights reserved.
 * SPDX-License-Identifier : Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
**/

#ifdef NONSECURE_TEST_BUILD
#include "val_interfaces.h"
#include "val_target.h"
#else
#include "val_client_defs.h"
#include "val_service_defs.h"
#endif

#include "test_i051.h"

const client_test_t test_i051_client_tests_list[] = {
    NULL,
    client_test_psa_call_with_invalid_outvec_base,
    NULL,
};

int32_t client_test_psa_call_with_invalid_outvec_base(caller_security_t caller)
{
   int32_t                 status = VAL_STATUS_SUCCESS;

   psa_status_t            status_of_call;
   boot_state_t            boot_state;
   memory_desc_t           *memory_desc;
   addr_t                  *invalid_base = NULL;

   val->print(PRINT_TEST,
            "[Check 1] Test psa_call with invalid psa_outvec.base\n", 0);

   /*
    * This test checks for the PROGRAMMER ERROR condition for the PSA API. API's respond to
    * PROGRAMMER ERROR could be either to return appropriate status code or panic the caller.
    * When a Secure Partition panics, the SPE cannot continue normal execution, as defined
    * in this specification. The behavior of the SPM following a Secure Partition panic is
    * IMPLEMENTATION DEFINED- Arm recommends that the SPM causes the system to restart in
    * this situation. Refer PSA-FF for more information on panic.
    * For the cases where, SPM cannot capable to reboot the system (just hangs or power down),
    * a watchdog timer set by val_test_init can reboot the system on timeout event. This will
    * tests continuity and able to jump to next tests. Therefore, each test who checks for
    * PROGRAMMER ERROR condition, expects system to get reset either by SPM or watchdog set by
    * the test harness function.
    *
    * If programmed timeout value isn't sufficient for your system, it can be reconfigured using
    * timeout entries available in target.cfg.
    *
    * To decide, a reboot happened as intended by test scenario or it happended
    * due to other reasons, test is setting a boot signature into non-volatile memory before and
    * after targeted test check. After a reboot, these boot signatures are being read by the
    * VAL APIs to decide test status.
    */

#if STATELESS_ROT != 1
   psa_handle_t            handle = 0;
   handle = psa->connect(SERVER_UNSPECIFED_VERSION_SID, SERVER_UNSPECIFED_VERSION_VERSION);
   if (!PSA_HANDLE_IS_VALID(handle))
   {
       val->print(PRINT_ERROR, "\tConnection failed\n", 0);
       return VAL_STATUS_INVALID_HANDLE;
   }
#endif

   /*
    * Selection of invalid outvec pointer:
    *
    * if caller == CALLER_NONSECURE
    *    // PSA RoT pointer
    *    invalid_base = driver_mmio_region_base;
    * else
    *    if (ISOLATION_LEVEL > 1)
    *      // PSA RoT pointer
    *       invalid_base = driver_mmio_region_base;
    *    else
    *       invalid_base = NULL;
    */

   status = val->target_get_config(TARGET_CONFIG_CREATE_ID(GROUP_MEMORY,
                                  MEMORY_DRIVER_PARTITION_MMIO, 0),
                                  (uint8_t **)&memory_desc,
                                  (uint32_t *)sizeof(memory_desc_t));
   if (val->err_check_set(TEST_CHECKPOINT_NUM(101), status))
   {
#if STATELESS_ROT != 1
       psa->close(handle);
#endif
       return status;
   }

   if (caller == CALLER_NONSECURE)
       invalid_base = (addr_t *) memory_desc->start;
   else
   {
       if (PLATFORM_PSA_ISOLATION_LEVEL > LEVEL1)
           invalid_base = (addr_t *) memory_desc->start;
   }

   /* Setting boot.state before test check */
   boot_state = (caller == CALLER_NONSECURE) ? BOOT_EXPECTED_NS : BOOT_EXPECTED_S;
   if (val->set_boot_flag(boot_state))
   {
       val->print(PRINT_ERROR, "\tFailed to set boot flag before check\n", 0);
       return VAL_STATUS_ERROR;
   }

   psa_outvec outvec[1] = {{invalid_base, sizeof(addr_t)}};

   /* Test check- psa_call with invalid address for psa_outvec.base */
#if STATELESS_ROT == 1
   status_of_call =  psa->call(SERVER_UNSPECIFED_VERSION_HANDLE, PSA_IPC_CALL, NULL, 0, outvec, 1);
#else
   status_of_call =  psa->call(handle, PSA_IPC_CALL, NULL, 0, outvec, 1);
#endif
   /*
    * If the caller is in the NSPE, it is IMPLEMENTATION DEFINED whether
    * a PROGRAMMER ERROR will panic or return PSA_ERROR_PROGRAMMER_ERROR.
    * For SPE caller, it must panic.
    */
   if (caller == CALLER_NONSECURE && status_of_call == PSA_ERROR_PROGRAMMER_ERROR)
   {
#if STATELESS_ROT != 1
       psa->close(handle);
#endif
       return VAL_STATUS_SUCCESS;
   }

   /* If PROGRAMMER ERROR results into panic then control shouldn't have reached here */
   val->print(PRINT_ERROR, "\tpsa_call should failed but succeed\n", 0);

   /* Resetting boot.state to catch unwanted reboot */
   if (val->set_boot_flag(BOOT_EXPECTED_BUT_FAILED))
   {
       val->print(PRINT_ERROR, "\tFailed to set boot flag after check\n", 0);
       return VAL_STATUS_ERROR;
   }

   status = VAL_STATUS_SPM_FAILED;
#if STATELESS_ROT != 1
       psa->close(handle);
#endif

   return status;
}
