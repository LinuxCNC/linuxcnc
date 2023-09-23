CREATE TABLE spindles (
    spindleID     INTEGER PRIMARY KEY,
    description   TEXT,
    machineID     TEXT DEFAULT "MachineID?",
    active        BOOLEAN DEFAULT TRUE,
    toolID        INTEGER DEFAULT NULL,
    offsetID      INTEGER,
    spindle_hrs   REAL DEFAULT 0.0
);

-- magazines table
CREATE TABLE magazines (
    magazineID  INTEGER PRIMARY KEY,
    description TEXT DEFAULT "",
    spindleID   INTEGER DEFAULT 0,
    type        TEXT DEFAULT "rotary",
    num_pockets INTEGER DEFAULT 1,
    base_pos    INTEGER
);

-- pockets table
CREATE TABLE pockets (
    magazineID  INTEGER DEFAULT 0,
    pocketID    INTEGER DEFAULT 0,
    toolID      TEXT DEFAULT NULL,
    pocket_offs INTEGER DEFAULT NULL,
    slot_pos    INTEGER DEFAULT NULL,
    PRIMARY KEY (magazineID, pocketID, toolID)
);

-- geom_groups table
CREATE TABLE geom_groups (
    groupID     INTEGER,
    description TEXT DEFAULT "",
    offsetID    INTEGER DEFAULT NULL,
    geomID      INTEGER DEFAULT NULL,
    PRIMARY KEY (groupID, description)
);

-- geometries table
CREATE TABLE geometries (
    geomID      INTEGER PRIMARY KEY,
    description TEXT DEFAULT "",
    orientation INTEGER DEFAULT NULL,
    frontangle  REAL DEFAULT NULL,
    backangle   REAL DEFAULT NULL
);

-- offsets table
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

-- tools table
CREATE TABLE tools (
    toolID      TEXT PRIMARY KEY,
    T_number    INTEGER,
    geom        INTEGER DEFAULT NULL,
    spindle_hrs REAL DEFAULT 0.0,
    distance    REAL DEFAULT 0.0,
    in_use      BOOLEAN DEFAULT 1,
    max_rpm     REAL DEFAULT -1
);


