
BEGIN TRANSACTION;

DROP TABLE IF EXISTS tools;
DROP TABLE IF EXISTS offsets;
DROP TABLE IF EXISTS geometries;
DROP TABLE IF EXISTS geom_groups;
DROP TABLE IF EXISTS magazines;
DROP TABLE IF EXISTS spindles;
DROP TABLE IF EXISTS pockets;
DROP TABLE IF EXISTS mag_types;

CREATE TABLE spindles (
    spindleID     INTEGER PRIMARY KEY,
    description   TEXT,
    toolID        INTEGER DEFAULT NULL,
    spindle_hrs   REAL DEFAULT 0.0
);
    
CREATE TABLE tools (
    toolID	INTEGER PRIMARY KEY,
    T_number	INTEGER,
    geom 	INTEGER DEFAULT NULL,
    spindle_hrs REAL DEFAULT 0.0,
    distance    REAL DEFAULT 0.0
);

CREATE TABLE offsets (
    offsetID    INTEGER PRIMARY KEY, 
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
    D           REAL DEFAULT 0.0
);

CREATE TABLE geometries (
    geomID      INTEGER PRIMARY KEY,
    description TEXT DEFAULT "",
    orientation INTEGER DEFAULT NULL,
    frontangle  REAL DEFAULT NULL,
    backangle   REAL DEFAULT NULL
);

CREATE TABLE geom_groups (
    groupID     INTEGER,
    description TEXT DEFAULT "",
    offsetID    INTEGER DEFAULT NULL,
    geomID      INTEGER DEFAULT NULL,
    PRIMARY KEY (groupID, description)
);

/* Tools are a Key to allow multiple tools per pocket, 
ie for Gang Tooling */
CREATE TABLE pockets (
    magazineID  INTEGER DEFAULT 0,
    pocketID    INTEGER DEFAULT 0,
    toolID      INTEGER DEFAULT NULL,
    slot_pos    INTEGER DEFAULT NULL,
    pocket_offs INTEGER DEFAULT NULL,
    PRIMARY KEY (magazineID, pocketID, toolID)
);

CREATE TABLE mag_types (type TEXT);
INSERT INTO mag_types(type) VALUES("rotary");
INSERT INTO mag_types(type) VALUES("linear");

CREATE TABLE magazines (
    magazineID  INTEGER PRIMARY KEY,
    description TEXT DEFAULT "",
    type        mag_type DEFAULT "rotary",
    num_pockets INTEGER DEFAULT 1,
    base_pos    INTEGER
);

COMMIT;
