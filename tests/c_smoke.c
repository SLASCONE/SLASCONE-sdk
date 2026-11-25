#include "SlasconeClient.h"

#include <stddef.h>

int main(void)
{
    if (slascone_client_abi_version() != 1)
        return 1;

    SlasconeClient *client = client_create(NULL);
    if (client == NULL)
        return 2;

    client_destroy(client);
    client_destroy(NULL);
    return 0;
}
