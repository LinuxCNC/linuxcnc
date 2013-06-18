
BEGIN TRANSACTION;

DROP TABLE IF EXISTS tools;
DROP TABLE IF EXISTS offsets;
DROP TABLE IF EXISTS geometries;
DROP TABLE IF EXISTS geom_groups;
DROP TABLE IF EXISTS magazines;
DROP TABLE IF EXISTS spindles;
DROP TABLE IF EXISTS pockets;
DROP TABLE IF EXISTS mag_types;
   
CREATE TABLE tools (
    pocket	    INTEGER PRIMARY KEY,
    tool	    INTEGER,
    description TEXT DEFAULT "",
    X           REAL DEFAULT 0.0,
    Y           REAL DEFAULT 0.0,
    Z           REAL DEFAULT 0.0,
    A           REAL DEFAULT 0.0,
    B           REAL DEFAULT 0.0,
    C           REAL DEFAULT 0.0,
    U           REAL DEFAULT 0.0,
    V           REAL DEFAULT 0.0,
    W           REAL DEFAULT 0.0,
    D           REAL DEFAULT 0.0,
    orientation INTEGER DEFAULT NULL,
    frontangle  REAL DEFAULT NULL,
    backangle   REAL DEFAULT NULL
);

COMMIT;
