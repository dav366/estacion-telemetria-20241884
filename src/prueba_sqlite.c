
#include <stdio.h>
#include "sqlite3.h"

int main(void)
{
    sqlite3 *db = NULL;

    int resultado = sqlite3_open(":memory:", &db);

    if (resultado != SQLITE_OK)
    {
        printf("Error al abrir SQLite: %s\n",
               sqlite3_errmsg(db));

        sqlite3_close(db);
        return 1;
    }

    printf("SQLite funciona correctamente.\n");

    sqlite3_close(db);

    return 0;
}
