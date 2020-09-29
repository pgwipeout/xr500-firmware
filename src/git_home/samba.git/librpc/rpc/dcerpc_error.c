/*
   Unix SMB/CIFS implementation.

   dcerpc fault functions

   Copyright (C) Stefan Metzmacher 2004

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "librpc/rpc/dcerpc.h"
#include "rpc_common.h"

struct dcerpc_fault_table {
	const char *errstr;
	uint32_t faultcode;
	NTSTATUS nt_status;
};

static const struct dcerpc_fault_table dcerpc_faults[] =
{
#define _FAULT_STR(x, s) { .errstr = #x , .faultcode = x, .nt_status = s }
#define _FAULT_STR_NO_NT_MAPPING(x) _FAULT_STR(x, NT_STATUS_RPC_NOT_RPC_ERROR)
	_FAULT_STR(DCERPC_NCA_S_COMM_FAILURE, NT_STATUS_RPC_COMM_FAILURE),
	_FAULT_STR(DCERPC_NCA_S_OP_RNG_ERROR, NT_STATUS_RPC_PROCNUM_OUT_OF_RANGE),
	_FAULT_STR(DCERPC_NCA_S_UNKNOWN_IF, NT_STATUS_RPC_UNKNOWN_IF),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_WRONG_BOOT_TIME),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_YOU_CRASHED),
	_FAULT_STR(DCERPC_NCA_S_PROTO_ERROR, NT_STATUS_RPC_PROTOCOL_ERROR),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_OUT_ARGS_TOO_BIG),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_SERVER_TOO_BUSY),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_STRING_TOO_LARGE),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_UNSUPPORTED_TYPE),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_ADDR_ERROR),
	_FAULT_STR(DCERPC_NCA_S_FAULT_FP_DIV_BY_ZERO, NT_STATUS_RPC_FP_DIV_ZERO),
	_FAULT_STR(DCERPC_NCA_S_FAULT_FP_UNDERFLOW, NT_STATUS_RPC_FP_UNDERFLOW),
	_FAULT_STR(DCERPC_NCA_S_FAULT_FP_OVERRFLOW, NT_STATUS_RPC_FP_OVERFLOW),
	_FAULT_STR(DCERPC_NCA_S_FAULT_INT_DIV_BY_ZERO, NT_STATUS_RPC_FP_DIV_ZERO),
	_FAULT_STR(DCERPC_NCA_S_FAULT_INT_OVERFLOW, NT_STATUS_RPC_FP_OVERFLOW),
	/*
	 * Our callers expect NT_STATUS_RPC_ENUM_VALUE_OUT_OF_RANGE
	 * instead of NT_STATUS_RPC_INVALID_TAG.
	 */
	_FAULT_STR(DCERPC_NCA_S_FAULT_INVALID_TAG, NT_STATUS_RPC_ENUM_VALUE_OUT_OF_RANGE),
	_FAULT_STR(DCERPC_NCA_S_FAULT_INVALID_TAG, NT_STATUS_RPC_INVALID_TAG),
	_FAULT_STR(DCERPC_NCA_S_FAULT_INVALID_BOUND, NT_STATUS_RPC_INVALID_BOUND),
	_FAULT_STR(DCERPC_NCA_S_FAULT_RPC_VERSION_MISMATCH, NT_STATUS_RPC_PROTOCOL_ERROR),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_UNSPEC_REJECT),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_BAD_ACTID),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_WHO_ARE_YOU_FAILED),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_MANAGER_NOT_ENTERED),
	_FAULT_STR(DCERPC_NCA_S_FAULT_CANCEL, NT_STATUS_RPC_CALL_CANCELLED),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_ILL_INST),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_FP_ERROR),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_UNUSED_1C000011),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_UNSPEC),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_REMOTE_COMM_FAILURE),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_PIPE_EMPTY),
	_FAULT_STR(DCERPC_NCA_S_FAULT_PIPE_CLOSED, NT_STATUS_RPC_PIPE_CLOSED),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_PIPE_ORDER),
	_FAULT_STR(DCERPC_NCA_S_FAULT_PIPE_DISCIPLINE, NT_STATUS_RPC_PIPE_DISCIPLINE_ERROR),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_PIPE_COMM_ERROR),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_PIPE_MEMORY),
	_FAULT_STR(DCERPC_NCA_S_FAULT_CONTEXT_MISMATCH, NT_STATUS_RPC_SS_CONTEXT_MISMATCH),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_REMOTE_NO_MEMORY),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_INVALID_PRES_CONTEXT_ID),
	_FAULT_STR(DCERPC_NCA_S_UNSUPPORTED_AUTHN_LEVEL, NT_STATUS_RPC_UNSUPPORTED_AUTHN_LEVEL),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_UNUSED_1C00001E),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_INVALID_CHECKSUM),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_INVALID_CRC),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_USER_DEFINED),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_TX_OPEN_FAILED),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_CODESET_CONV_ERROR),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_OBJECT_NOT_FOUND),
	_FAULT_STR_NO_NT_MAPPING(DCERPC_NCA_S_FAULT_NO_CLIENT_STUB),
	_FAULT_STR(DCERPC_FAULT_OTHER, NT_STATUS_RPC_CALL_FAILED),
	_FAULT_STR(DCERPC_FAULT_ACCESS_DENIED, NT_STATUS_ACCESS_DENIED),
	_FAULT_STR(DCERPC_FAULT_SERVER_UNAVAILABLE, NT_STATUS_RPC_SERVER_UNAVAILABLE),
	_FAULT_STR(DCERPC_FAULT_NO_CALL_ACTIVE, NT_STATUS_RPC_NO_CALL_ACTIVE),
	_FAULT_STR(DCERPC_FAULT_CANT_PERFORM, NT_STATUS_EPT_CANT_PERFORM_OP),
	_FAULT_STR(DCERPC_FAULT_OUT_OF_RESOURCES, NT_STATUS_RPC_OUT_OF_RESOURCES),
	_FAULT_STR(DCERPC_FAULT_BAD_STUB_DATA, NT_STATUS_RPC_BAD_STUB_DATA),
	_FAULT_STR(DCERPC_FAULT_SEC_PKG_ERROR, NT_STATUS_RPC_SEC_PKG_ERROR),
	{ NULL, 0 }
#undef _FAULT_STR
};

_PUBLIC_ const char *dcerpc_errstr(TALLOC_CTX *mem_ctx, uint32_t fault_code)
{
	int idx = 0;
	WERROR werr = W_ERROR(fault_code);

	while (dcerpc_faults[idx].errstr != NULL) {
		if (dcerpc_faults[idx].faultcode == fault_code) {
			return dcerpc_faults[idx].errstr;
		}
		idx++;
	}

	return win_errstr(werr);
}

_PUBLIC_ NTSTATUS dcerpc_fault_to_nt_status(uint32_t fault_code)
{
	int idx = 0;
	WERROR werr = W_ERROR(fault_code);

	if (fault_code == 0) {
		return NT_STATUS_RPC_PROTOCOL_ERROR;
	}

	while (dcerpc_faults[idx].errstr != NULL) {
		if (dcerpc_faults[idx].faultcode == fault_code) {
			return dcerpc_faults[idx].nt_status;
		}
		idx++;
	}

	return werror_to_ntstatus(werr);
}

_PUBLIC_ uint32_t dcerpc_fault_from_nt_status(NTSTATUS nt_status)
{
	int idx = 0;
	WERROR werr;

	if (NT_STATUS_IS_OK(nt_status)) {
		return DCERPC_NCA_S_PROTO_ERROR;
	}

	while (dcerpc_faults[idx].errstr != NULL) {
		if (NT_STATUS_EQUAL(dcerpc_faults[idx].nt_status, nt_status)) {
			return dcerpc_faults[idx].faultcode;
		}
		idx++;
	}

	werr = ntstatus_to_werror(nt_status);

	return W_ERROR_V(werr);
}
