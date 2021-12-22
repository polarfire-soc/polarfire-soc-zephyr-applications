/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <bluetooth/gatt.h>
#include <bluetooth/services/ots.h>
#include "ots_internal.h"
#include "ots_dir_list_internal.h"

#include <logging/log.h>

LOG_MODULE_DECLARE(bt_ots, CONFIG_BT_OTS_LOG_LEVEL);

#define OACP_PROC_TYPE_SIZE	1
#define OACP_RES_MAX_SIZE	3

#if defined(CONFIG_BT_OTS_OACP_WRITE_SUPPORT)
static ssize_t oacp_write_proc_cb(struct bt_gatt_ots_l2cap *l2cap_ctx,
			struct bt_conn *conn, struct net_buf *buf);

static void oacp_l2cap_closed(struct bt_gatt_ots_l2cap *l2cap_ctx,
			struct bt_conn *conn)
{
	struct bt_ots *ots;

	ots = CONTAINER_OF(l2cap_ctx, struct bt_ots, l2cap);

	if (!ots->cur_obj) {
		return;
	}

	ots->cur_obj->state.type = BT_GATT_OTS_OBJECT_IDLE_STATE;
	l2cap_ctx->rx_done = NULL;
	l2cap_ctx->tx_done = NULL;
}
#endif

static enum bt_gatt_ots_oacp_res_code oacp_read_proc_validate(
	struct bt_conn *conn,
	struct bt_ots *ots,
	struct bt_gatt_ots_oacp_proc *proc)
{
	struct bt_gatt_ots_oacp_read_params *params = &proc->read_params;

	LOG_DBG("Validating Read procedure with offset: 0x%08X and "
		"length: 0x%08X", params->offset, params->len);

	if (!ots->cur_obj) {
		return BT_GATT_OTS_OACP_RES_INV_OBJ;
	}

	if (!BT_OTS_OBJ_GET_PROP_READ(ots->cur_obj->metadata.props)) {
		return BT_GATT_OTS_OACP_RES_NOT_PERMITTED;
	}

	if (!bt_gatt_ots_l2cap_is_open(&ots->l2cap, conn)) {
		return BT_GATT_OTS_OACP_RES_CHAN_UNAVAIL;
	}

	if ((params->offset + (uint64_t) params->len) >
		ots->cur_obj->metadata.size.cur) {
		return BT_GATT_OTS_OACP_RES_INV_PARAM;
	}

	if (ots->cur_obj->state.type != BT_GATT_OTS_OBJECT_IDLE_STATE) {
		return BT_GATT_OTS_OACP_RES_OBJ_LOCKED;
	}

	ots->cur_obj->state.type = BT_GATT_OTS_OBJECT_READ_OP_STATE;
	ots->cur_obj->state.read_op.sent_len = 0;
	memcpy(&ots->cur_obj->state.read_op.oacp_params, &proc->read_params,
		sizeof(ots->cur_obj->state.read_op.oacp_params));

	LOG_DBG("Read procedure is accepted");

	return BT_GATT_OTS_OACP_RES_SUCCESS;
}

#if defined(CONFIG_BT_OTS_OACP_WRITE_SUPPORT)
static enum bt_gatt_ots_oacp_res_code oacp_write_proc_validate(
	struct bt_conn *conn,
	struct bt_ots *ots,
	struct bt_gatt_ots_oacp_proc *proc)
{
	struct bt_gatt_ots_oacp_write_params *params = &proc->write_params;

	LOG_DBG("Validating Write procedure with offset: 0x%08X and "
		"length: 0x%08X", params->offset, params->len);

	if (!ots->cur_obj) {
		return BT_GATT_OTS_OACP_RES_INV_OBJ;
	}

	if (!BT_OTS_OBJ_GET_PROP_WRITE(ots->cur_obj->metadata.props)) {
		return BT_GATT_OTS_OACP_RES_NOT_PERMITTED;
	}

	/* patching is attempted */
	if (params->offset < ots->cur_obj->metadata.size.cur) {
		if (!BT_OTS_OACP_GET_FEAT_PATCH(ots->features.oacp)) {
			return BT_GATT_OTS_OACP_RES_NOT_PERMITTED;
		}
		if (!BT_OTS_OBJ_GET_PROP_PATCH(ots->cur_obj->metadata.props)) {
			return BT_GATT_OTS_OACP_RES_NOT_PERMITTED;
		}
	}

	/* truncation is not supported */
	if (BT_GATT_OTS_OACP_PROC_WRITE_MODE_GET_TRUNC(params->mode)) {
		return BT_GATT_OTS_OACP_RES_NOT_PERMITTED;
	}

	if (!bt_gatt_ots_l2cap_is_open(&ots->l2cap, conn)) {
		return BT_GATT_OTS_OACP_RES_CHAN_UNAVAIL;
	}

	if (BT_GATT_OTS_OACP_PROC_WRITE_MODE_GET_RFU(params->mode)) {
		return BT_GATT_OTS_OACP_RES_INV_PARAM;
	}

	if (params->offset > ots->cur_obj->metadata.size.cur) {
		return BT_GATT_OTS_OACP_RES_INV_PARAM;
	}

	/* append is not supported */
	if ((params->offset + (uint64_t) params->len) > ots->cur_obj->metadata.size.alloc) {
		return BT_GATT_OTS_OACP_RES_INV_PARAM;
	}

	if (ots->cur_obj->state.type != BT_GATT_OTS_OBJECT_IDLE_STATE) {
		return BT_GATT_OTS_OACP_RES_OBJ_LOCKED;
	}

	ots->l2cap.rx_done = oacp_write_proc_cb;
	ots->l2cap.closed = oacp_l2cap_closed;
	ots->cur_obj->state.type = BT_GATT_OTS_OBJECT_WRITE_OP_STATE;
	ots->cur_obj->state.write_op.recv_len = 0;
	memcpy(&ots->cur_obj->state.write_op.oacp_params, params,
		sizeof(ots->cur_obj->state.write_op.oacp_params));

	LOG_DBG("Write procedure is accepted");

	return BT_GATT_OTS_OACP_RES_SUCCESS;
}
#endif

static enum bt_gatt_ots_oacp_res_code oacp_proc_validate(
	struct bt_conn *conn,
	struct bt_ots *ots,
	struct bt_gatt_ots_oacp_proc *proc)
{
	switch (proc->type) {
	case BT_GATT_OTS_OACP_PROC_READ:
		return oacp_read_proc_validate(conn, ots, proc);
	case BT_GATT_OTS_OACP_PROC_WRITE:
#if defined(CONFIG_BT_OTS_OACP_WRITE_SUPPORT)
		return oacp_write_proc_validate(conn, ots, proc);
#endif
	case BT_GATT_OTS_OACP_PROC_CREATE:
	case BT_GATT_OTS_OACP_PROC_DELETE:
	case BT_GATT_OTS_OACP_PROC_CHECKSUM_CALC:
	case BT_GATT_OTS_OACP_PROC_EXECUTE:
	case BT_GATT_OTS_OACP_PROC_ABORT:
	default:
		return BT_GATT_OTS_OACP_RES_OPCODE_NOT_SUP;
	}
};

static enum bt_gatt_ots_oacp_res_code oacp_command_decode(
	const uint8_t *buf, uint16_t len,
	struct bt_gatt_ots_oacp_proc *proc)
{
	struct net_buf_simple net_buf;

	net_buf_simple_init_with_data(&net_buf, (void *) buf, len);

	proc->type = net_buf_simple_pull_u8(&net_buf);
	switch (proc->type) {
	case BT_GATT_OTS_OACP_PROC_CREATE:
		proc->create_params.size = net_buf_simple_pull_le32(&net_buf);
		bt_uuid_create(&proc->create_params.type.uuid, net_buf.data,
			       net_buf.len);
		net_buf_simple_pull_mem(&net_buf, net_buf.len);
		break;
	case BT_GATT_OTS_OACP_PROC_DELETE:
		break;
	case BT_GATT_OTS_OACP_PROC_CHECKSUM_CALC:
		proc->cs_calc_params.offset =
			net_buf_simple_pull_le32(&net_buf);
		proc->cs_calc_params.len =
			net_buf_simple_pull_le32(&net_buf);
		break;
	case BT_GATT_OTS_OACP_PROC_EXECUTE:
		break;
	case BT_GATT_OTS_OACP_PROC_READ:
		proc->read_params.offset =
			net_buf_simple_pull_le32(&net_buf);
		proc->read_params.len =
			net_buf_simple_pull_le32(&net_buf);
		return BT_GATT_OTS_OACP_RES_SUCCESS;
	case BT_GATT_OTS_OACP_PROC_WRITE:
#if defined(CONFIG_BT_OTS_OACP_WRITE_SUPPORT)
		proc->write_params.offset =
			net_buf_simple_pull_le32(&net_buf);
		proc->write_params.len =
			net_buf_simple_pull_le32(&net_buf);
		proc->write_params.mode =
			net_buf_simple_pull_u8(&net_buf);
		return BT_GATT_OTS_OACP_RES_SUCCESS;
#else
		break;
#endif
	case BT_GATT_OTS_OACP_PROC_ABORT:
	default:
		break;
	}

	LOG_WRN("OACP unsupported procedure type");
	return BT_GATT_OTS_OACP_RES_OPCODE_NOT_SUP;
}

static bool oacp_command_len_verify(struct bt_gatt_ots_oacp_proc *proc,
				    uint16_t len)
{
	uint16_t ref_len = OACP_PROC_TYPE_SIZE;

	switch (proc->type) {
	case BT_GATT_OTS_OACP_PROC_CREATE:
	{
		struct bt_ots_obj_type *type = &proc->create_params.type;

		switch (type->uuid.type) {
		case BT_UUID_TYPE_16:
			ref_len += BT_GATT_OTS_OACP_CREATE_UUID16_PARAMS_SIZE;
			break;
		case BT_UUID_TYPE_32:
			ref_len += BT_GATT_OTS_OACP_CREATE_UUID32_PARAMS_SIZE;
			break;
		case BT_UUID_TYPE_128:
			ref_len += BT_GATT_OTS_OACP_CREATE_UUID128_PARAMS_SIZE;
			break;
		default:
			return false;
		}
	} break;
	case BT_GATT_OTS_OACP_PROC_DELETE:
		break;
	case BT_GATT_OTS_OACP_PROC_CHECKSUM_CALC:
		ref_len += BT_GATT_OTS_OACP_CS_CALC_PARAMS_SIZE;
		break;
	case BT_GATT_OTS_OACP_PROC_EXECUTE:
		break;
	case BT_GATT_OTS_OACP_PROC_READ:
		ref_len += BT_GATT_OTS_OACP_READ_PARAMS_SIZE;
		break;
	case BT_GATT_OTS_OACP_PROC_WRITE:
		ref_len += BT_GATT_OTS_OACP_WRITE_PARAMS_SIZE;
		break;
	case BT_GATT_OTS_OACP_PROC_ABORT:
		break;
	default:
		return true;
	}

	return (len == ref_len);
}

static void oacp_read_proc_cb(struct bt_gatt_ots_l2cap *l2cap_ctx,
			      struct bt_conn *conn)
{
	int err;
	void *obj_chunk;
	off_t offset;
	ssize_t len;
	struct bt_ots *ots;
	struct bt_gatt_ots_object_read_op *read_op;

	ots     = CONTAINER_OF(l2cap_ctx, struct bt_ots, l2cap);
	read_op = &ots->cur_obj->state.read_op;
	offset  = read_op->oacp_params.offset + read_op->sent_len;

	if (read_op->sent_len >= read_op->oacp_params.len) {
		LOG_DBG("OACP Read Op over L2CAP is completed");

		if (read_op->sent_len > read_op->oacp_params.len) {
			LOG_WRN("More bytes sent that the client requested");
		}

		ots->cur_obj->state.type = BT_GATT_OTS_OBJECT_IDLE_STATE;

		if (IS_ENABLED(CONFIG_BT_OTS_DIR_LIST_OBJ) &&
		    ots->cur_obj->id == OTS_OBJ_ID_DIR_LIST) {
			return;
		}

		ots->cb->obj_read(ots, conn, ots->cur_obj->id, NULL, 0,
				  offset);
		return;
	}

	len = read_op->oacp_params.len - read_op->sent_len;
	if (IS_ENABLED(CONFIG_BT_OTS_DIR_LIST_OBJ) &&
	    ots->cur_obj->id == OTS_OBJ_ID_DIR_LIST) {
		len = bt_ots_dir_list_content_get(ots->dir_list, &obj_chunk, len, offset);
	} else {
		len = ots->cb->obj_read(ots, conn, ots->cur_obj->id, &obj_chunk,
					len, offset);
	}

	if (len < 0) {
		LOG_ERR("OCAP Read Op failed with error: %zd", len);

		bt_gatt_ots_l2cap_disconnect(&ots->l2cap);
		ots->cur_obj->state.type = BT_GATT_OTS_OBJECT_IDLE_STATE;

		return;
	}

	ots->l2cap.tx_done = oacp_read_proc_cb;
	err = bt_gatt_ots_l2cap_send(&ots->l2cap, obj_chunk, len);
	if (err) {
		LOG_ERR("L2CAP CoC error: %d while trying to execute OACP "
			"Read procedure", err);
		ots->cur_obj->state.type = BT_GATT_OTS_OBJECT_IDLE_STATE;
	} else {
		read_op->sent_len += len;
	}
}

static void oacp_read_proc_execute(struct bt_ots *ots,
				   struct bt_conn *conn)
{
	struct bt_gatt_ots_oacp_read_params *params =
		&ots->cur_obj->state.read_op.oacp_params;

	if (!ots->cur_obj) {
		LOG_ERR("Invalid Current Object on OACP Read procedure");
		return;
	}

	LOG_DBG("Executing Read procedure with offset: 0x%08X and "
		"length: 0x%08X", params->offset, params->len);

	if (IS_ENABLED(CONFIG_BT_OTS_DIR_LIST_OBJ) &&
	    ots->cur_obj->id == OTS_OBJ_ID_DIR_LIST) {
		oacp_read_proc_cb(&ots->l2cap, conn);
	} else if (ots->cb->obj_read) {
		oacp_read_proc_cb(&ots->l2cap, conn);
	} else {
		ots->cur_obj->state.type = BT_GATT_OTS_OBJECT_IDLE_STATE;
		LOG_ERR("OTS Read operation failed: "
			"there is no OTS Read callback");
	}
}

#if defined(CONFIG_BT_OTS_OACP_WRITE_SUPPORT)
static ssize_t oacp_write_proc_cb(struct bt_gatt_ots_l2cap *l2cap_ctx,
			struct bt_conn *conn, struct net_buf *buf)
{
	struct bt_gatt_ots_object_write_op *write_op;
	struct bt_ots *ots;
	off_t offset;
	size_t rem;
	size_t len;
	ssize_t rc;

	ots = CONTAINER_OF(l2cap_ctx, struct bt_ots, l2cap);

	if (!ots->cur_obj) {
		LOG_ERR("Invalid Current Object on OACP Write procedure");
		return -ENODEV;
	}

	if (!ots->cb->obj_write) {
		LOG_ERR("OTS Write operation failed: "
			"there is no OTS Write callback");
		ots->cur_obj->state.type = BT_GATT_OTS_OBJECT_IDLE_STATE;
		return -ENODEV;
	}

	write_op = &ots->cur_obj->state.write_op;
	offset = write_op->oacp_params.offset + write_op->recv_len;
	len = buf->len;
	if (write_op->recv_len + len > write_op->oacp_params.len) {
		LOG_WRN("More bytes received than the client indicated");
		len = write_op->oacp_params.len - write_op->recv_len;
	}
	rem = write_op->oacp_params.len - (write_op->recv_len + len);

	rc = ots->cb->obj_write(ots, conn, ots->cur_obj->id, buf->data, len,
				  offset, rem);

	if (rc < 0) {
		len = 0;

		/*
		 * Returning an EINPROGRESS return code results in the write buffer not being
		 * released by the l2cap layer. This is an unsupported use case at the moment.
		 */
		if (rc == -EINPROGRESS) {
			LOG_ERR("Unsupported error code %zd returned by object write callback", rc);
		}

		LOG_ERR("OTS Write operation failed with error: %zd", rc);
		ots->cur_obj->state.type = BT_GATT_OTS_OBJECT_IDLE_STATE;
	} else {
		/* Return -EIO as an error if all of data was not written */
		if (rc != len) {
			len = rc;
			rc = -EIO;
		}
	}

	write_op->recv_len += len;
	if (write_op->recv_len == write_op->oacp_params.len) {
		LOG_DBG("OACP Write Op over L2CAP is completed");
		ots->cur_obj->state.type = BT_GATT_OTS_OBJECT_IDLE_STATE;
	}

	if (offset + len > ots->cur_obj->metadata.size.cur) {
		ots->cur_obj->metadata.size.cur = offset + len;
	}

	return rc;
}
#endif

static void oacp_ind_cb(struct bt_conn *conn,
			struct bt_gatt_indicate_params *params,
			uint8_t err)
{
	struct bt_ots *ots = (struct bt_ots *) params->attr->user_data;

	LOG_DBG("Received OACP Indication ACK with status: 0x%04X", err);

	switch (ots->cur_obj->state.type) {
	case BT_GATT_OTS_OBJECT_READ_OP_STATE:
		oacp_read_proc_execute(ots, conn);
		break;
	case BT_GATT_OTS_OBJECT_WRITE_OP_STATE:
		/* procedure execution is driven by L2CAP socket receive */
		break;
	default:
		LOG_ERR("Unsupported OTS state: %d", ots->cur_obj->state.type);
		break;
	}
}

static int oacp_ind_send(const struct bt_gatt_attr *oacp_attr,
			 enum bt_gatt_ots_oacp_proc_type req_op_code,
			 enum bt_gatt_ots_oacp_res_code oacp_status)
{
	uint8_t oacp_res[OACP_RES_MAX_SIZE];
	uint16_t oacp_res_len = 0;
	struct bt_ots *ots = (struct bt_ots *) oacp_attr->user_data;

	/* Encode OACP Response */
	oacp_res[oacp_res_len++] = BT_GATT_OTS_OACP_PROC_RESP;
	oacp_res[oacp_res_len++] = req_op_code;
	oacp_res[oacp_res_len++] = oacp_status;

	/* Prepare indication parameters */
	memset(&ots->oacp_ind.params, 0, sizeof(ots->oacp_ind.params));
	memcpy(&ots->oacp_ind.attr, oacp_attr, sizeof(ots->oacp_ind.attr));
	ots->oacp_ind.params.attr = &ots->oacp_ind.attr;
	ots->oacp_ind.params.func = oacp_ind_cb;
	ots->oacp_ind.params.data = oacp_res;
	ots->oacp_ind.params.len  = oacp_res_len;

	LOG_DBG("Sending OACP indication");

	return bt_gatt_indicate(NULL, &ots->oacp_ind.params);
}

ssize_t bt_gatt_ots_oacp_write(struct bt_conn *conn,
				   const struct bt_gatt_attr *attr,
				   const void *buf, uint16_t len,
				   uint16_t offset, uint8_t flags)
{
	enum bt_gatt_ots_oacp_res_code oacp_status;
	struct bt_gatt_ots_oacp_proc oacp_proc;
	struct bt_ots *ots = (struct bt_ots *) attr->user_data;

	LOG_DBG("Object Action Control Point GATT Write Operation");

	if (!ots->oacp_ind.is_enabled) {
		LOG_WRN("OACP indications not enabled");
		return BT_GATT_ERR(BT_ATT_ERR_CCC_IMPROPER_CONF);
	}

	if (offset != 0) {
		LOG_ERR("Invalid offset of OACP Write Request");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	oacp_status = oacp_command_decode(buf, len, &oacp_proc);

	if (!oacp_command_len_verify(&oacp_proc, len)) {
		LOG_ERR("Invalid length of OACP Write Request for 0x%02X "
			"Op Code", oacp_proc.type);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if (oacp_status == BT_GATT_OTS_OACP_RES_SUCCESS) {
		oacp_status = oacp_proc_validate(conn, ots, &oacp_proc);
	}

	if (oacp_status != BT_GATT_OTS_OACP_RES_SUCCESS) {
		LOG_WRN("OACP Write error status: 0x%02X", oacp_status);
	}

	oacp_ind_send(attr, oacp_proc.type, oacp_status);
	return len;
}

void bt_gatt_ots_oacp_cfg_changed(const struct bt_gatt_attr *attr,
				      uint16_t value)
{
	struct bt_gatt_ots_indicate *oacp_ind =
	    CONTAINER_OF((struct _bt_gatt_ccc *) attr->user_data,
			 struct bt_gatt_ots_indicate, ccc);

	LOG_DBG("Object Action Control Point CCCD value: 0x%04X", value);

	oacp_ind->is_enabled = false;
	if (value == BT_GATT_CCC_INDICATE) {
		oacp_ind->is_enabled = true;
	}
}
