-- Insert geometries for new tools (assuming they all have the same geometry)
INSERT INTO geometries (description, orientation, frontangle, backangle) 
VALUES 
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0),
("End Mill", 0, 0.0, 0.0);

-- Insert Z offsets for tools
INSERT INTO offsets (description, X, Y, Z, A, B, C, U, V, W, D) 
VALUES 
("Tool0 Offset", 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0),
("Tool1 Offset", 0.0, 0.0, 3.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0),
("Tool2 Offset", 0.0, 0.0, 3.2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3.0),
("Tool3 Offset", 0.0, 0.0, 3.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 4.0),
("Tool4 Offset", 0.0, 0.0, 3.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 5.0),
("Tool5 Offset", 0.0, 0.0, 3.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 6.0),
("Tool6 Offset", 0.0, 0.0, 3.6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 7.0),
("Tool7 Offset", 0.0, 0.0, 3.7, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 8.0),
("Tool8 Offset", 0.0, 0.0, 3.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 9.0),
("Tool9 Offset", 0.0, 0.0, 3.9, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10.0),
("Tool10 Offset", 0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 11.0),
("Tool11 Offset", 0.0, 0.0, 4.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 12.0),
("Tool12 Offset", 0.0, 0.0, 4.2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 13.0),
("Tool13 Offset", 0.0, 0.0, 4.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 14.0),
("Tool14 Offset", 0.0, 0.0, 4.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 15.0),
("Tool15 Offset", 0.0, 0.0, 4.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 16.0),
("Tool16 Offset", 0.0, 0.0, 4.6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 17.0),
("Tool17 Offset", 0.0, 0.0, 4.7, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 18.0),
("Tool18 Offset", 0.0, 0.0, 4.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 19.0),
("Tool19 Offset", 0.0, 0.0, 4.9, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 20.0);

-- Insert the tools with the desired naming
INSERT INTO tools (toolID, T_number, geom, spindle_hrs, distance, in_use, max_rpm) 
VALUES 
("Tool0", 0, (SELECT MAX(geomID) - 19 FROM geometries), 0.0, 1.0, 1, -1),
("Tool1", 1, (SELECT MAX(geomID) - 18 FROM geometries), 0.0, 2.0, 1, -1),
("Tool2", 2, (SELECT MAX(geomID) - 17 FROM geometries), 0.0, 3.0, 1, -1),
("Tool3", 3, (SELECT MAX(geomID) - 16 FROM geometries), 0.0, 4.0, 1, -1),
("Tool4", 4, (SELECT MAX(geomID) - 15 FROM geometries), 0.0, 5.0, 1, -1),
("Tool5", 5, (SELECT MAX(geomID) - 14 FROM geometries), 0.0, 6.0, 1, -1),
("Tool6", 6, (SELECT MAX(geomID) - 13 FROM geometries), 0.0, 7.0, 1, -1),
("Tool7", 7, (SELECT MAX(geomID) - 12 FROM geometries), 0.0, 8.0, 1, -1),
("Tool8", 8, (SELECT MAX(geomID) - 11 FROM geometries), 0.0, 9.0, 1, -1),
("Tool9", 9, (SELECT MAX(geomID) - 10 FROM geometries), 0.0, 10.0, 1, -1),
("Tool10", 10, (SELECT MAX(geomID) - 9 FROM geometries), 0.0, 11.0, 1, -1),
("Tool11", 11, (SELECT MAX(geomID) - 8 FROM geometries), 0.0, 12.0, 1, -1),
("Tool12", 12, (SELECT MAX(geomID) - 7 FROM geometries), 0.0, 13.0, 1, -1),
("Tool13", 13, (SELECT MAX(geomID) - 6 FROM geometries), 0.0, 14.0, 1, -1),
("Tool14", 14, (SELECT MAX(geomID) - 5 FROM geometries), 0.0, 15.0, 1, -1),
("Tool15", 15, (SELECT MAX(geomID) - 4 FROM geometries), 0.0, 16.0, 1, -1),
("Tool16", 16, (SELECT MAX(geomID) - 3 FROM geometries), 0.0, 17.0, 1, -1),
("Tool17", 17, (SELECT MAX(geomID) - 2 FROM geometries), 0.0, 18.0, 1, -1),
("Tool18", 18, (SELECT MAX(geomID) - 1 FROM geometries), 0.0, 19.0, 1, -1),
("Tool19", 19, (SELECT MAX(geomID) FROM geometries), 0.0, 20.0, 1, -1);
