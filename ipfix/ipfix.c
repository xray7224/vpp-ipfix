/*
 * Copyright (c) 2017 Igalia
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * @file
 * @brief IPFIX Plugin, plugin API / trace / CLI handling.
 */

#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <ipfix/ipfix.h>

#include <vlibapi/api.h>
#include <vlibmemory/api.h>
#include <vlibsocket/api.h>

/* define message IDs */
#include <ipfix/ipfix_msg_enum.h>

/* define message structures */
#define vl_typedefs
#include <ipfix/ipfix_all_api_h.h> 
#undef vl_typedefs

/* define generated endian-swappers */
#define vl_endianfun
#include <ipfix/ipfix_all_api_h.h> 
#undef vl_endianfun

/* instantiate all the print functions we know about */
#define vl_print(handle, ...) vlib_cli_output (handle, __VA_ARGS__)
#define vl_printfun
#include <ipfix/ipfix_all_api_h.h> 
#undef vl_printfun

/* Get the API version number */
#define vl_api_version(n,v) static u32 api_version=(v);
#include <ipfix/ipfix_all_api_h.h>
#undef vl_api_version

#define REPLY_MSG_ID_BASE sm->msg_id_base
#include <vlibapi/api_helper_macros.h>

/* List of message types that this plugin understands */

#define foreach_ipfix_plugin_api_msg                           \
_(IPFIX_MACSWAP_ENABLE_DISABLE, ipfix_macswap_enable_disable)

/* *INDENT-OFF* */
VLIB_PLUGIN_REGISTER () = {
    .version = IPFIX_PLUGIN_BUILD_VER,
    .description = "Ipfix of VPP Plugin",
};
/* *INDENT-ON* */

/**
 * @brief Enable/disable the macswap plugin. 
 *
 * Action function shared between message handler and debug CLI.
 */

int ipfix_macswap_enable_disable (ipfix_main_t * sm, u32 sw_if_index,
                                   int enable_disable)
{
  vnet_sw_interface_t * sw;
  int rv = 0;

  /* Utterly wrong? */
  if (pool_is_free_index (sm->vnet_main->interface_main.sw_interfaces, 
                          sw_if_index))
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;

  /* Not a physical port? */
  sw = vnet_get_sw_interface (sm->vnet_main, sw_if_index);
  if (sw->type != VNET_SW_INTERFACE_TYPE_HARDWARE)
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;
  
  vnet_feature_enable_disable ("ip4-unicast", "ipfix",
                               sw_if_index, enable_disable, 0, 0);

  return rv;
}

static clib_error_t *
macswap_enable_disable_command_fn (vlib_main_t * vm,
                                   unformat_input_t * input,
                                   vlib_cli_command_t * cmd)
{
  ipfix_main_t * sm = &ipfix_main;
  u32 sw_if_index = ~0;
  int enable_disable = 1;
    
  int rv;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT) {
    if (unformat (input, "disable"))
      enable_disable = 0;
    else if (unformat (input, "%U", unformat_vnet_sw_interface,
                       sm->vnet_main, &sw_if_index))
      ;
    else
      break;
  }

  if (sw_if_index == ~0)
    return clib_error_return (0, "Please specify an interface...");
    
  rv = ipfix_macswap_enable_disable (sm, sw_if_index, enable_disable);

  switch(rv) {
  case 0:
    break;

  case VNET_API_ERROR_INVALID_SW_IF_INDEX:
    return clib_error_return 
      (0, "Invalid interface, only works on physical ports");
    break;

  case VNET_API_ERROR_UNIMPLEMENTED:
    return clib_error_return (0, "Device driver doesn't support redirection");
    break;

  default:
    return clib_error_return (0, "ipfix_macswap_enable_disable returned %d",
                              rv);
  }
  return 0;
}

/**
 * @brief CLI command to enable/disable the ipfix macswap plugin.
 */
VLIB_CLI_COMMAND (sr_content_command, static) = {
    .path = "ipfix macswap",
    .short_help = 
    "ipfix macswap <interface-name> [disable]",
    .function = macswap_enable_disable_command_fn,
};

/**
 * @brief Plugin API message handler.
 */
static void vl_api_ipfix_macswap_enable_disable_t_handler
(vl_api_ipfix_macswap_enable_disable_t * mp)
{
  vl_api_ipfix_macswap_enable_disable_reply_t * rmp;
  ipfix_main_t * sm = &ipfix_main;
  int rv;

  rv = ipfix_macswap_enable_disable (sm, ntohl(mp->sw_if_index), 
                                      (int) (mp->enable_disable));
  
  REPLY_MACRO(VL_API_IPFIX_MACSWAP_ENABLE_DISABLE_REPLY);
}

/**
 * @brief Set up the API message handling tables.
 */
static clib_error_t *
ipfix_plugin_api_hookup (vlib_main_t *vm)
{
  ipfix_main_t * sm = &ipfix_main;
#define _(N,n)                                                  \
    vl_msg_api_set_handlers((VL_API_##N + sm->msg_id_base),     \
                           #n,					\
                           vl_api_##n##_t_handler,              \
                           vl_noop_handler,                     \
                           vl_api_##n##_t_endian,               \
                           vl_api_##n##_t_print,                \
                           sizeof(vl_api_##n##_t), 1); 
    foreach_ipfix_plugin_api_msg;
#undef _

    return 0;
}

#define vl_msg_name_crc_list
#include <ipfix/ipfix_all_api_h.h>
#undef vl_msg_name_crc_list

static void 
setup_message_id_table (ipfix_main_t * sm, api_main_t *am)
{
#define _(id,n,crc) \
  vl_msg_api_add_msg_name_crc (am, #n "_" #crc, id + sm->msg_id_base);
  foreach_vl_msg_name_crc_ipfix;
#undef _
}

/**
 * @brief Initialize the ipfix plugin.
 */
static clib_error_t * ipfix_init (vlib_main_t * vm)
{
  ipfix_main_t * sm = &ipfix_main;
  clib_error_t * error = 0;
  u8 * name;

  sm->vnet_main =  vnet_get_main ();

  name = format (0, "ipfix_%08x%c", api_version, 0);

  /* Ask for a correctly-sized block of API message decode slots */
  sm->msg_id_base = vl_msg_api_get_msg_ids 
      ((char *) name, VL_MSG_FIRST_AVAILABLE);

  /* Initialize IP vector */
  sm->ip_vec = 0;

  error = ipfix_plugin_api_hookup (vm);

  /* Add our API messages to the global name_crc hash table */
  setup_message_id_table (sm, &api_main);

  vec_free(name);

  return error;
}

VLIB_INIT_FUNCTION (ipfix_init);

/**
 * @brief Hook the ipfix plugin into the VPP graph hierarchy.
 */
VNET_FEATURE_INIT (ipfix, static) = 
{
  .arc_name = "ip4-unicast",
  .node_name = "ipfix",
  .runs_before = VNET_FEATURES ("ip4-lookup"),
};
