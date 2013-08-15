
BEGIN TRANSACTION;

DROP TABLE IF EXISTS "tools";
CREATE TABLE "tools" (
    toolno	INTEGER PRIMARY KEY,
    pocket	INTEGER,
    diameter 	REAL DEFAULT (0.0),
    backangle   REAL DEFAULT (0.0),
    frontangle  REAL DEFAULT (0.0),
    orientation INTEGER DEFAULT (0.0),
    comment	 TEXT DEFAULT (NULL),
    x_offset REAL DEFAULT (0.0),
    y_offset REAL DEFAULT (0.0),
    z_offset REAL DEFAULT (0.0),
    a_offset REAL DEFAULT (0.0),
    b_offset REAL DEFAULT (0.0),
    c_offset REAL DEFAULT (0.0),
    u_offset REAL DEFAULT (0.0),
    v_offset REAL DEFAULT (0.0),
    w_offset REAL DEFAULT (0.0)
);



INSERT INTO "tools" VALUES (1,2,4.0, 0.0,0.0,0.0,"tool1",1.2,0.0,3.7,0.0,0.0,0.0,0.0,0.0,0.0);
INSERT INTO "tools" VALUES (2,1,2.0, 0.0,0.0,0.0,"tool2",0.2,0.0,1.2,0.0,0.0,0.0,0.0,0.0,0.0);
INSERT INTO "tools" VALUES (3,7,3.3, 0.0,0.0,0.0,"tool3",3.2,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0);
INSERT INTO "tools" VALUES (4,3,1.2, 0.0,0.0,0.0,"tool4",0.0,0.0,5.9,0.0,0.0,0.0,0.0,0.0,0.0);
INSERT INTO "tools" VALUES (5,5,6.2, 0.0,0.0,0.0,"tool5",0.0,0.0,1.1,0.0,0.0,0.0,0.0,0.0,0.0);
INSERT INTO "tools" VALUES (6,9,1.9, 0.0,0.0,0.0,"tool6",0.0,0.0,3.4,0.0,0.0,0.0,0.0,0.0,0.0);
INSERT INTO "tools" VALUES (7,8,0.3, 0.0,0.0,0.0,"tool7",0.0,0.0,1.7,0.0,0.0,0.0,0.0,0.0,0.0);
INSERT INTO "tools" VALUES (8,11,2.50, 0.0,0.0,0.0,"tool8",0.0,0.0,2.7,0.0,0.0,0.0,0.0,0.0,0.0);
INSERT INTO "tools" VALUES (9,24,1.1, 0.0,0.0,0.0,"tool9",0.0,0.0,4.9,0.0,0.0,0.0,0.0,0.0,0.0);

DROP TABLE IF EXISTS "state";
CREATE TABLE "state" (
    tool_in_spindle	INTEGER,
    pocket_prepped	INTEGER

);

INSERT INTO "state" VALUES (6,9);



COMMIT;
