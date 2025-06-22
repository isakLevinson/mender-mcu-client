    path "pki_int/issue/*" {
      capabilities = ["create", "update"]
    }

    path "pki_int/certs" {
      capabilities = ["list"]
    }

    path "sys/leases/renew" {
      capabilities = ["update"]
    }
		
    path "pki_int/revoke" {
      capabilities = ["create", "update"]
    }

    path "pki_int/tidy" {
      capabilities = ["create", "update"]
    }

    path "pki/cert/ca" {
      capabilities = ["read"]
    }

   path "pki_int/cert/ca" {
      capabilities = ["read"]
    }

    path "auth/token/renew" {
      capabilities = ["update"]
    }

    path "auth/token/renew-self" {
      capabilities = ["update"]
    }

    path "pki_int/roles/*" {
      capabilities = ["read", "list"]
    }

    path "pki_int/roles" {
      capabilities = ["list"]
    }

    path "sys/mounts/*" {
	capabilities = ["create", "update", "delete"]
    }

    path "secret/*" {
      capabilities = ["list", "read"]
    }

