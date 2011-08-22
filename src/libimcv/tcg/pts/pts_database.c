/*
 * Copyright (C) 2011 Sansar Choinyambuu
 * HSR Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "pts_database.h"

#include <debug.h>
#include <crypto/hashers/hasher.h>


typedef struct private_pts_database_t private_pts_database_t;

/**
 * Private data of a pts_database_t object.
 *
 */
struct private_pts_database_t {

	/**
	 * Public pts_database_t interface.
	 */
	pts_database_t public;

	/**
	 * database instance
	 */
	database_t *db;

};

METHOD(pts_database_t, create_file_enumerator, enumerator_t*,
	private_pts_database_t *this, char *product, char *version)
{
	enumerator_t *e = NULL;

	return e;
}

METHOD(pts_database_t, destroy, void,
	private_pts_database_t *this)
{
	this->db->destroy(this->db);
	free(this);
}

/**
 * See header
 */
pts_database_t *pts_database_create(char *uri)
{
	private_pts_database_t *this;

	INIT(this,
		.public = {
			.create_file_enumerator = _create_file_enumerator,
			.destroy = _destroy,
		},
		.db = lib->db->create(lib->db, uri),
	);

	if (!this->db)
	{
		DBG1(DBG_TNC, "pts failed to connect to file database");
		free(this);
		return NULL;
	}

	return &this->public;
}

