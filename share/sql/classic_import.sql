/*CLASSIC IMPORT SCRIPT. Emulates EMC2 style offsets
Valid tags are :Tool :Pocket :X :Y :Z :U :V: :W :A :B :C 
 :Diameter, :Frontangle :Backangle :Orientation :Comment: */
BEGIN TRANSACTION;
INSERT INTO tools (pocket, tool, description, X, Y, Z, A, B, C, U, V, W, D,
                   orientation, frontangle, backangle) 
           VALUES (:Pocket, :Tool, ":Comment", :X, :Y, :Z, :A, :B, :C,
                   :U, :V, :W, :Diameter,
                   :Orientation, :Frontangle, :Backangle);
                   
COMMIT;
     
            
