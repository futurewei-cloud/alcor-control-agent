// Copyright (c) 2014 Open Networking Foundation
// Copyright 2020 Futurewei Cloud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/** @file Functions for secure communication using SSL */
#ifndef __SSL_IMPL_HH__
#define __SSL_IMPL_HH__

namespace fluid_base {
	/** SSL implementation pointer for internal library use. */
	extern void* tls_obj;

	/** Initialize SSL parameters. You must call this function before
		asking any object to communicate in a secure manner.

		@param cert The controller's certificate signed by a CA
		@param privkey The controller's private key to be used with the
					   certificate
		@param trustedcert A CA certificate that signs certificates of trusted
		   switches */
	void libfluid_tls_init(const char* cert, const char* privkey, const char* trustedcert);

	/** Free SSL data. You must call this function after you don't need secure
		communication anymore. */
	void libfluid_tls_clear();
}

#endif