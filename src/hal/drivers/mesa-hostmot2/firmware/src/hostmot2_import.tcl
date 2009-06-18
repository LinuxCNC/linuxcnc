# ProjectNavigator SourceControl recreation script
#
# This script is text version of significant (but possibly not all)
# the information contained in the ISE project file.  It is generated
# and used by the ProjectNavigator application's source control
# import feature.
#
# When using this script from the command line to recreate the ISE
# project, it should first be sourced from within an xtclsh shell.
# Next, the import procedure should be called to perform the import.
# When calling the import procedure, pass the new project directory
# and the source directory.  If neither are specified, the current
# working directory is assumed for both.
#
# Internally this script has two file lists. One variable (import_files)
# has the set of files to copy into the project directory.  The other
# variable (user_files) has the set of files to add into the project.
#
#
# This script is not intended for direct customer editing.
#
# Copyright 2006, Xilinx, Inc.
#


#  Helper to copy files from the source staging area
#  back into the destination work area.
#  This proc will be call for each file copied.
#  While not supported, one could do interesting things with this
#  proc, since each file hits it.
proc CopyIn { srcfile work_area copy_option } {
   set staging_area [pwd]
   if { [ expr { [ file pathtype $srcfile ] == "absolute"   || \
                 [string index $srcfile 0 ] == "/"        || \
                 [string index $srcfile 1 ] == ":"        } ] } {
       set workfile $srcfile
   } else {
       set workfile [ file join $work_area $srcfile ]
   }
   if { $copy_option == "flatten" } {
       set stagefile [ file join $staging_area [ file tail $srcfile ] ]
   } elseif { [ file pathtype $srcfile ] != "relative" } {
       set srcfile [ string map {: _} $srcfile ]
       set stagefile [ file join $staging_area absolute $srcfile ]
   } elseif { [ expr { $copy_option == "absremote" } && { [string equal -length 2 $srcfile ".." ] } ] } {
       set stagefile [ file join $staging_area outside_relative [ string map {.. up} $srcfile ] ]
   } else {
       set srcfile [ string map {: _} $srcfile ]
       set stagefile [ file join $staging_area $srcfile ]
   }

   set stagefile [ file normalize $stagefile ]
   set workfile [ file normalize $workfile ]

   if { [ file exists $stagefile ] } {
      if { $stagefile != $workfile } {
         file mkdir [ file dirname $workfile ]
         file copy -force $stagefile $workfile
      }
   } else { WARN "\"$stagefile\" does not exist for import copy." }
}

proc ERR { msg } {
   puts "ERROR: $msg"
}

proc WARN { msg } {
   puts "WARNING: $msg"
}

proc INFO { msg } {
   puts "$msg"
}

# Helper that returns 1 if the string is blank, otherwise 0.
proc IsBlank { str } {
   if { [string length $str] == 0 } {
      return 1
   }
   return 0
}

# Helper for determining whether a value is 'NULL'.
# Returns 1 if the value is 0; returns 0 if the value is anything else.
proc IsNull { val } {
   if { $val == 0 } {
      return 1
   }
   return 0
}

proc HandleException { script { msg "" } } {
   set catch_result [catch {
      uplevel 1 $script
   } RESULT]
   if {$catch_result} {
      if {![IsBlank $msg]} {
         ERR $msg
      }
      INFO "$RESULT"
      INFO "$::errorInfo"
   }
}

# These two procs help to load shared libraries in a platform
# independent way.
proc _LoadLibrary {name} {
   set libExt [info sharedlibextension]
   set libFullName "$name$libExt"
   HandleException {
      load $libFullName
   } "A problem occured loading library $libFullName."
}

proc _LoadFactoryLibrary {Factory} {
   HandleException {
      Xilinx::Cit::FactoryLoad $Factory
   } "A problem occured loading library $Factory."
}

_LoadLibrary libCit_CoreStub
_LoadLibrary libPrjrep_CommonStub
_LoadFactoryLibrary libPrjrep_Common
_LoadLibrary libDpm_SupportStub
_LoadLibrary libDpm_PnfStub
_LoadLibrary libDpm_DefnDataStub
_LoadLibrary libDpm_DesignDataStub
_LoadLibrary libDpm_HdlStub
_LoadLibrary libPrjrep_RepositoryStub
_LoadLibrary libCitI_CoreStub
_LoadLibrary libHdcI_HdcHDProjectStub
_LoadLibrary libTcltaskI_TaskStub
_LoadLibrary libCommonI_CommonStub
_LoadFactoryLibrary libTcltask_Helpers
_LoadFactoryLibrary libHdcC_HDProject
_LoadLibrary libHdcI_HdcContainerStub

#  Helper to exectute code only when the (pointer) variable name is valid.
proc OnOkPtr { var_name script } {
   if { [ uplevel info exists $var_name ] } {
      upvar $var_name var
      if { $var != 0 } { return [ uplevel $script ] }
   }
}

#  Helper to exectute code only when the (pointer) variable name is 0.
proc OnNullPtr { var_name script } {
   if { [ uplevel info exists $var_name ] } {
      upvar $var_name var
      if { $var == 0 } { return [ uplevel $script ] }
   }
}

#  Helper to exectute code only when the value of variable name is 1.
proc OnSuccess { var_name script } {
   if { $val != 0 } { return [ uplevel $script ] }
}

#  Helper to exectute code only when the value of variable name is 0.
proc OnFail { val script } {
   if { $val != 1 } { return [ uplevel $script ] }
}

#  Helper to get a component interface.
proc GetInterface { iUnk id { name "" } } {
   if {$iUnk == 0} { return 0 }
   set iIface [ $iUnk GetInterface $id ]
   OnNullPtr iIface {
      if {![IsBlank $name]} {
         ERR " Could not get the \"$name\" interface."
      }
   }
   return $iIface
}

#  Helper to create a component and return one of its interfaces.
proc CreateComponent { compId ifaceId { name "" } } {
   set iUnk [ ::Xilinx::Cit::FactoryCreate $compId ]
   set iIface [ GetInterface $iUnk $ifaceId ]
   OnNullPtr iIface {
      if {![IsBlank $name]} { ERR "Could not create a \"$name\" component." }
   }
   return $iIface
}

#  Helper to release an object
proc Release { args } {
   foreach iUnk $args {
      set i_refcount [ GetInterface $iUnk $::xilinx::Prjrep::IRefCountID ]
      OnNullPtr i_refcount { set i_refcount [ GetInterface $iUnk $::xilinx::CommonI::IRefCountID ] }
      OnOkPtr i_refcount { $i_refcount Release }
   }
}

#  Helper to loop over IIterator based pointers.
proc ForEachIterEle { _ele_var_name _iter script } {
   if {$_iter == 0} { return 0 }
   upvar $_ele_var_name ele
   for { $_iter First } { ![ $_iter IsEnd ] } { $_iter Next }  {
      set ele [ $_iter CurrentItem ]
      set returned_val [ uplevel $script ] 
   }
}

#  Helper to get the Tcl Project Manager, if possible.
proc GetTclProjectMgr { } {
   set TclProjectMgrId "{7d528480-1196-4635-aba9-639446e4aa59}"
   set iUnk [ Xilinx::CitP::CreateComponent $TclProjectMgrId ]
   if {$iUnk == 0} { return 0 }
   set iTclProjectMgr [ $iUnk GetInterface $::xilinx::TcltaskI::ITclProjectMgrID ]
   OnNullPtr iTclProjectMgr {
      ERR "Could not create a \"TclProjectMgr\" component."
   }
   return $iTclProjectMgr
}

#  Helper to get the current Tcl Project, if one is open.
proc GetCurrentTclProject { } {
   set iTclProject 0
   set iTclProjectMgr [GetTclProjectMgr]
   OnOkPtr iTclProjectMgr {
      set errmsg ""
      $iTclProjectMgr GetCurrentTclProject iTclProject errmsg
   }
   return $iTclProject
}

#  Helper to get the current HDProject, if one is open.
proc GetCurrentHDProject { } {
   set iHDProject 0
   set iTclProjectMgr [GetTclProjectMgr]
   set errmsg ""
   OnOkPtr iTclProjectMgr { $iTclProjectMgr GetCurrentHDProject iHDProject errmsg }
   OnNullPtr iHDProject {
      ERR "Could not get the current HDProject."
   }
   return $iHDProject
}

#  Helper to create a Project Helper.
proc GetProjectHelper { } {
   set ProjectHelperID "{0725c3d2-5e9b-4383-a7b6-a80c932eac21}"
   set iProjHelper [CreateComponent $ProjectHelperID $::xilinx::Dpm::IProjectHelperID "Project Helper"]
   return $iProjHelper
}

#  Helper to find out if a project is currently open.
#  Returns 1 if a project is open, otherwise 0.
proc IsProjectOpen { } {
   set iTclProject [GetCurrentTclProject]
   set isOpen [expr {$iTclProject != 0}]
   Release $iTclProject
   return $isOpen
}

#  Helper to return the lock file for the specified project if there is one.
#  Returns an empty string if there is no lock file on the specified project,
#  or there is no corresponding .ise file
#  This assumes that the project_file is in the current directory.
#  It also assumes project_file does not have a path.
proc GetProjectLockFile { project_file } {
   if { ![ file isfile "$project_file" ] } {
      return 
   }
   INFO "Checking for a lock file for \"$project_file\"."
   set lock_file "__ISE_repository_${project_file}_.lock"
   if { [ file isfile "$lock_file" ] } {
      return $lock_file
   }
   return 
}

#  Helper to back up the project file.
#  This assumes that the project_file is in the current directory.
proc BackUpProject { project_file backup_file } {
   if { ![ file isfile "$project_file" ] } {
      WARN "Could not find \"$project_file\"; the project will not be backed up."
   return 0
   } else {
      INFO "Backing up the project to \"$backup_file\"."
      file copy -force "$project_file" "$backup_file"
   }
   return 1
}

#  Helper to remove the project file so that a new project can be created
#  in its place. Presumably the old project is corrupted and can no longer
#  be opened.
proc RemoveProject { project_file } {
   file delete -force "$project_file"
   # Return failure if the project still exists.
   if { [ file isfile "$project_file" ] } {
      ERR "Could not remove \"$project_file\"; Unable to restore the project."
      return 0
   }
   return 1
}

#  Helper to open a project and return a project facilitator (pointer).
proc OpenFacilProject { project_name } {
   # first make sure the tcl project mgr singleton exists
   GetTclProjectMgr
   # get a Project Helper and open the project.
   set iProjHelper [GetProjectHelper]
   if {$iProjHelper == 0} { return 0 }
   set result [$iProjHelper Open $project_name]
   OnFail $result {
      if {$result == 576460769483292673} {
         ERR "Could not open the project \"$project_name\" because it is locked."
      } else {
         ERR "Could not open the \"$project_name\" project."
      }
      Release $iProjHelper
      set iProjHelper 0
   }
   return $iProjHelper
}

#  Helper to close and release a project.
proc CloseFacilProject { iProjHelper } {
   if {$iProjHelper == 0} { return }
   $iProjHelper Close
   Release $iProjHelper
}

#  Helper to get the Project from the Project Helper.
#  Clients must release this.
proc GetProject { iProjHelper } {
   if {$iProjHelper == 0} { return 0 }
   set dpm_project 0
   $iProjHelper GetDpmProject dpm_project
   set iProject [ GetInterface $dpm_project $xilinx::Dpm::IProjectID ]
   OnNullPtr iProject {
      ERR "Could not get the Project from the Project Helper."
   }
   return $iProject
}

#  Helper to get the File Manager from the Project Helper.
#  Clients must release this.
proc GetFileManager { iProjHelper } {
   set iProject [GetProject $iProjHelper]
   set iFileMgr [ GetInterface $iProject $xilinx::Dpm::IFileManagerID ]
   OnNullPtr iFileMgr {
      ERR "Could not get the File Manager from the Project Helper."
   }
   # Don't release the project here, clients will release it 
   # when they release its IFileManager interface.
   return $iFileMgr
}

#  Helper to get the Source Library Manager from the Project Helper.
#  Clients must release this.
proc GetSourceLibraryManager { iProjHelper } {
   set iProject [GetProject $iProjHelper]
   set iSourceLibraryMgr [ GetInterface $iProject $xilinx::Dpm::ISourceLibraryManagerID ]
   OnNullPtr iSourceLibraryMgr {
      ERR "Could not get the Source Library Manager from the Project Helper."
   }
   # Don't release the project here, clients will release it 
   # when they release its IFileManager interface.
   return $iSourceLibraryMgr
}

#  Helper to get the ProjSrcHelper from the Project Helper.
#  Clients must NOT release this.
proc GetProjSrcHelper { iProjHelper } {
   set iSrcHelper [ GetInterface $iProjHelper $::xilinx::Dpm::IProjSrcHelperID IProjSrcHelper ]
   OnNullPtr iSrcHelper {
      ERR "Could not get the ProjSrcHelper from the Project Helper."
   }
   return $iSrcHelper
}

#  Helper to get the ScratchPropertyManager from the Project Helper.
#  Clients must NOT release this.
proc GetScratchPropertyManager { iProjHelper } {
   set iPropTableFetch [ GetInterface $iProjHelper $xilinx::Dpm::IPropTableFetchID IPropTableFetch ]
   set prop_table_comp 0
   OnOkPtr iPropTableFetch {
      $iPropTableFetch GetPropTable prop_table_comp
   }
   set iScratch [ GetInterface $prop_table_comp $xilinx::Dpm::IScratchPropertyManagerID ]
   OnNullPtr iScratch {
      ERR "Could not get the Scratch Property Manager from the Project Helper."
   }
   return $iScratch
}

#  Helper to get the Design from the Project Helper.
#  Clients must release this.
proc GetDesign { iProjHelper } {
   set iProject [GetProject $iProjHelper]
   set iDesign 0
   OnOkPtr iProject { $iProject GetDesign iDesign }
   OnNullPtr iDesign {
      ERR "Could not get the Design from the Project Helper."
   }
   Release $iProject
   return $iDesign
}

#  Helper to get the Data Store from the Project Helper.
#  Clients must NOT release this.
proc GetDataStore { iProjHelper } {
   set iDesign [ GetDesign $iProjHelper]
   set iDataStore 0
   OnOkPtr iDesign { $iDesign GetDataStore iDataStore }
   OnNullPtr iDataStore {
      ERR "Could not get the Data Store from the Project Helper."
   }
   Release $iDesign
   return $iDataStore
}

#  Helper to get the View Manager from the Project Helper.
#  Clients must NOT release this.
proc GetViewManager { iProjHelper } {
   set iDesign [ GetDesign $iProjHelper]
   set iViewMgr [ GetInterface $iDesign $xilinx::Dpm::IViewManagerID ]
   OnNullPtr iViewMgr {
      ERR "Could not get the View Manager from the Project Helper."
   }
   # Don't release the design here, clients will release it 
   # when they release its IViewManager interface.
   return $iViewMgr
}

#  Helper to get the Property Manager from the Project Helper.
#  Clients must release this.
proc GetPropertyManager { iProjHelper } {
   set iDesign [ GetDesign $iProjHelper]
   set iPropMgr 0
   OnOkPtr iDesign { $iDesign GetPropertyManager iPropMgr }
   OnNullPtr iPropMgr {
      ERR "Could not get the Property Manager from the Project Helper."
   }
   Release $iDesign
   return $iPropMgr
}

#  Helper to find a property template, based on prop_name
#  Clients must NOT release this.
proc GetPropertyTemplate { iProjHelper prop_name } {
   set iPropTempl 0
   set iUnk 0
   set iDefdataId 0
   set iPropTemplStore 0
   set iDataStore [GetDataStore $iProjHelper]
   OnOkPtr iDataStore { $iDataStore GetComponentByName $prop_name iUnk }
   OnOkPtr iUnk { set iDefdataId [ GetInterface $iUnk $xilinx::Dpm::IDefDataIdID IDefDataId ] }
   OnOkPtr iDefdataId {
      set iPropTemplStore [ GetInterface $iDataStore $xilinx::Dpm::IPropertyTemplateStoreID IPropertyTemplateStore ]
   }
   OnOkPtr iPropTemplStore { $iPropTemplStore GetPropertyTemplate $iDefdataId iPropTempl }
   OnNullPtr iPropTempl {
      WARN "Could not get the property template for \"$prop_name\"."
   }
   return $iPropTempl
}

#  Helper to get a component's name.
proc GetName { iUnk } {
   set name ""
   set iName [ GetInterface $iUnk $xilinx::Prjrep::INameID IName ]
   OnOkPtr iName { $iName GetName name }
   return $name
}

#  Helper to get the name of a view's type.
proc GetViewTypeName { iView } {
   set typeName ""
   set iType 0
   set iDefdataType 0
   OnOkPtr iView { $iView GetType iType }
   OnOkPtr iType {
      set iDefdataType [ GetInterface $iType $xilinx::Dpm::IDefDataIdID IDefDataId ]
   }
   OnOkPtr iDefdataType { $iDefdataType GetID typeName }
   return $typeName
}

#  Helper to find a view and return its context.
#  Must clients release this?
proc GetViewContext { iProjHelper view_id view_name } {
   # Simply return if the view_id or view_name is empty.
   if { [IsBlank $view_id] || [IsBlank $view_name] } { return 0 }
   set foundview 0
   set viewiter 0
   set iViewMgr [GetViewManager $iProjHelper]
   OnOkPtr iViewMgr { $iViewMgr GetViews viewiter }
   ForEachIterEle view $viewiter {
      set typeName [GetViewTypeName $view]
      set name [GetName $view]
      if { [ string equal $name $view_name ] && [ string equal $view_id $typeName ] } {
         set foundview $view
      }
   }
   set context [ GetInterface $foundview $xilinx::Dpm::IPropertyContextID ]
   OnNullPtr context {
      WARN "Could not get the context for view \"$view_id\":\"$view_name\"."
   }
   return $context
}

#  Helper to get a string property instance from the property manager.
proc GetStringPropertyInstance { iProjHelper simple_id } {
   set iPropMgr [GetPropertyManager $iProjHelper]
   if {$iPropMgr == 0} { return 0 }
   set iPropInst 0
   $iPropMgr GetStringProperty $simple_id iPropInst
   OnNullPtr iPropInst { WARN "Could not get the string property instance $simple_id." }
   Release $iPropMgr
   return $iPropInst
}

#  Helper to get a property instance from the property manager.
proc GetPropertyInstance { iProjHelper view_name view_id prop_name } {
   set iPropInst 0
   set iPropTempl [ GetPropertyTemplate $iProjHelper $prop_name ]
   if {$iPropTempl == 0} { return 0 }
   set context [ GetViewContext $iProjHelper $view_id $view_name ]
   set iPropMgr [GetPropertyManager $iProjHelper]
   if {$iPropMgr == 0} { return 0 }
   $iPropMgr GetPropertyInstance $iPropTempl $context iPropInst
   OnNullPtr iPropInst {
      if { ![IsBlank $view_id] && ![IsBlank $view_name] } {
         WARN "Could not get the context sensitive property instance $prop_name."
      } else {
         WARN "Could not get the property instance $prop_name."
      }
   }
   Release $iPropMgr
   return $iPropInst
}

#  Helper to store properties back into the property manager.
proc RestoreProcessProperties { iProjHelper process_props } {
   INFO "Restoring process properties"
   foreach { unused view_name view_id simple_id prop_name prop_val } $process_props {
      set iPropInst 0
      if {![IsBlank $simple_id]} {
         set iPropInst [ GetStringPropertyInstance $iProjHelper $simple_id ]
      } else {
         set iPropInst [ GetPropertyInstance $iProjHelper $view_name $view_id $prop_name ]
      }
      OnOkPtr iPropInst {
         OnFail [ $iPropInst SetStringValue "$prop_val" ] {
            WARN "Could not set the value of the $prop_name property to \"$prop_val\"."
         }
      }
      Release $iPropInst
   }
}

#  Helper to recreate partitions from the variable name with
#  a list of instance names.
proc RestorePartitions { namelist } {
   INFO "Restoring partitions."
   set iHDProject [ GetCurrentHDProject ]
   OnOkPtr iHDProject {
      foreach name $namelist {
         set iPartition [ $iHDProject CreatePartition "$name" ]
      }
   }
}

#  Helper to create and populate a library
#
proc CreateLibrary { iProjHelper libname filelist } {

   set iLibMgr [ GetSourceLibraryManager $iProjHelper ]
   set iFileMgr [ GetFileManager $iProjHelper ]

   if {$iLibMgr == 0} { return 0 }
   if {$iFileMgr == 0} { return 0 }

   $iLibMgr CreateSourceLibrary "libname" ilib

   OnOkPtr ilib {
      foreach filename $filelist {
         set argfile [ file normalize "$filename" ]
         set found 0
         set fileiter 0
         $iFileMgr GetFiles fileiter
         ForEachIterEle ifile $fileiter {
            set path ""
            set file ""
            $ifile getPath path file
            set currentfile [ file normalize [ file join "$path" "$file" ] ]
            if { $currentfile == $argfile } {
               set found 1
               $ilib AddFile ifile
               break
            }
         }
         OnNullPtr found {
            WARN "Could not add the file \"$filename\" to the library \"$libname\"."
         }
      }
   }
}

#  Helper to create source libraries and populate them.
proc RestoreSourceLibraries { iProjHelper libraries } {
   INFO "Restoring source libraries."
   foreach { libname filelist } $libraries {
      CreateLibrary $iProjHelper "$libname" $filelist
   }
}

# Helper to add user files to the project using the PnF.
proc AddUserFiles { iProjHelper files } {
   INFO "Adding User files."
   set iconflict 0
   set iSrcHelper [ GetProjSrcHelper $iProjHelper ]
   if {$iSrcHelper == 0} { return 0 }
   foreach filename $files {
      INFO "Adding the file \"$filename\" to the project."
      set result [$iSrcHelper AddSourceFile "$filename" iconflict]
      OnFail $result {
         if {$result == 6} {
            INFO "The file \"$filename\" is already in the project."
         } else {
            ERR "A problem occurred adding the file \"$filename\" to the project."
         }
      }
   }
}

# Helper to add files to the project and set their origination. 
# Valid origination values are:
#   0 - User
#   1 - Generated
#   2 - Imported
# Files of origination "User" are added through the facilitator, 
# otherwise they are added directly to the File Manager.
proc AddImportedFiles { iProjHelper files origination } {
   switch $origination {
      0 { INFO "Adding User files." }
      1 { INFO "Adding Generated files." }
      2 { INFO "Adding Imported files." }
      default {
         ERR "Invalid parameter: origination was set to \"$origination\", but may only be 0, 1, or 2."
         return 0
      }
   }
   set iFileMgr [ GetFileManager $iProjHelper ]
   if {$iFileMgr == 0} { return 0 }
   foreach filename $files {
      set file_type 0
      set hdl_file 0
      set result [$iFileMgr AddFile "$filename" $file_type hdl_file]
      OnFail $result {
         if {$result == 6} {
            INFO "The file \"$filename\" is already in the project."
         } elseif { $hdl_file == 0 } {
            ERR "A problem occurred adding the file \"$filename\" to the project."
         }
      }
      OnOkPtr hdl_file {
         set ifile [ GetInterface $hdl_file $xilinx::Dpm::IFileID IFile ]
         OnOkPtr ifile {
            set result [ $ifile SetOrigination $origination ]
            if {$result != 1} {
               ERR "A problem occurred setting the origination of \"$filename\" to \"$origination\"."
            }
            Release $ifile
         }
      }
   }
   return 1
}

proc RestoreProjectSettings { iProjHelper project_settings } {
   INFO "Restoring device settings"
   set iScratch [GetScratchPropertyManager $iProjHelper]
   set iPropIter 0
   set iPropSet [ GetInterface $iScratch $xilinx::Dpm::IPropertyNodeSetID IPropertyNodeSet ]
   OnOkPtr iPropSet {
      $iPropSet GetIterator iPropIter
   }
   set index 0
   set lastindex [llength $project_settings]
   ForEachIterEle prop_node $iPropIter {
      set prop_instance 0
      $prop_node GetPropertyInstance prop_instance
      if { $index < $lastindex } {
         set argname [ lindex $project_settings $index ]
         set argvalue [ lindex $project_settings [ expr $index + 1 ] ]
      } else {
         set argname {}
         set argvalue {}
      }
      if { $prop_instance != 0 } {
         set name {}
         $prop_instance GetName name
         if { [string equal $name $argname ] } {
            $prop_instance SetStringValue $argvalue
            incr index
            incr index
         }
      }
      Release $prop_instance
   }
   $iScratch Commit
   # initialize
   $iProjHelper Init
}

#  Helper to load a source control configuration from a stream
#  and then store it back into an ise file.
proc RestoreSourceControlOptions { prjfile istream } {
   INFO "Restoring source control options"
   set config_comp [::Xilinx::Cit::FactoryCreate $::xilinx::Dpm::SourceControlConfigurationCompID ]
   OnOkPtr config_comp { set ipersist [ $config_comp GetInterface $xilinx::Prjrep::IPersistID ] }
   OnOkPtr config_comp { set igetopts [ $config_comp GetInterface $xilinx::Dpm::SrcCtrl::IGetOptionsID ] }
   set helper_comp [::Xilinx::Cit::FactoryCreate $::xilinx::Dpm::SourceControlHelpCompID ]
   OnOkPtr helper_comp { set ihelper [ $config_comp GetInterface $xilinx::Dpm::SrcCtrl::IHelperID ] }
   OnOkPtr ipersist { $ipersist Load istream }
   OnOkPtr ihelper { OnOkPtr igetopts { $ihelper SaveOptions $prjfile $igetopts } }
   Release $helper_comp $config_comp
}

proc import { {working_area ""} {staging_area ""} { srcctrl_comp 0 } } {
  set project_file "hostmot2.ise"
  set old_working_dir [pwd]
  # intialize the new project directory (work) and 
  # source control reference directory (staging) to
  # current working directory, when not specified
  if { $working_area == "" } { set working_area [pwd] }
  if { $staging_area == "" } { set staging_area [pwd] }
  set copy_option relative
  set import_files { 
      "4i65.ucf" 
      "4i68.ucf" 
      "5i20.ucf" 
      "5i22.ucf" 
      "5i23.ucf" 
      "7i43.ucf" 
      "7i43u.ucf" 
      "CountPinsInRange.vhd" 
      "IDROMConst.vhd" 
      "MaxPinsPerModule.vhd" 
      "NumberOfModules.vhd" 
      "PIN_24XQCTRONLY_72.vhd" 
      "PIN_2X7I65_72.vhd" 
      "PIN_ST12_72.vhd" 
      "PIN_SV12IM_2X7I48_72.vhd" 
      "PIN_SV12_72.vhd" 
      "PIN_SV16_96.vhd" 
      "PIN_SV8NA.vhd" 
      "PIN_SV8_48.vhd" 
      "PIN_SVSP4_6_7I46NA_48.vhd" 
      "PIN_SVSP6_2_48.vhd" 
      "PIN_SVSP8_6_7I46_72.vhd" 
      "PIN_SVSPD6_2_48.vhd" 
      "PIN_SVST2_4_7I47_48.vhd" 
      "PIN_SVST2_4_7I47_72.vhd" 
      "PIN_SVST2_8_72.vhd" 
      "PIN_SVST4_12NA_48.vhd" 
      "PIN_SVST4_12_48.vhd" 
      "PIN_SVST4_4NA_48.vhd" 
      "PIN_SVST4_4_48.vhd" 
      "PIN_SVST4_6NA_48.vhd" 
      "PIN_SVST4_6_48.vhd" 
      "PIN_SVST4_8_72.vhd" 
      "PIN_SVST8_24_96.vhd" 
      "PIN_SVST8_4IM2_72.vhd" 
      "PIN_SVST8_4_72.vhd" 
      "PIN_SVST8_8IM2_72.vhd" 
      "PIN_SVST8_8_96.vhd" 
      "PIN_SVSTSP8_12_6_96.vhd" 
      "PIN_SVUA8_4_72.vhd" 
      "PIN_SVUA8_8_72.vhd" 
      "Top9030HostMot2.vhd" 
      "Top9054HostMot2.vhd" 
      "TopEPPHostMot2.vhd" 
      "TopUSBHostMot2.vhd" 
      "atrans.vhd" 
      "b32test_guide.ncd" 
      "boutreg.vhd" 
      "bufferedspi.vhd" 
      "d8o8.vhd" 
      "dpll.vhd" 
      "hostmot2.vhd" 
      "hostmotid.vhd" 
      "i20card.vhd" 
      "i22_1000card.vhd" 
      "i22_1500card.vhd" 
      "i23card.vhd" 
      "i43_200card.vhd" 
      "i43_400card.vhd" 
      "i65card.vhd" 
      "i68card.vhd" 
      "idrom.vhd" 
      "irqlogic.vhd" 
      "kubstepgenz.vhd" 
      "pwmpdmgenh.vhd" 
      "pwmrefh.vhd" 
      "qcounterate.vhd" 
      "qcountersf.vhd" 
      "simplespi8.vhd" 
      "simplespix.vhd" 
      "simplessi.vhd" 
      "testram.vhd" 
      "testrom.vhd" 
      "timestamp.vhd" 
      "uartr.vhd" 
      "uartr8.vhd" 
      "uartx.vhd" 
      "uartx8.vhd" 
      "ubrategen.vhd" 
      "usbram.vhd" 
      "usbrom.vhd" 
      "watchdog.vhd" 
      "wordpr.vhd" 
      "wordrb.vhd"}
  INFO "Copying files from \"$staging_area\" to \"$working_area\""
  # Must be in the staging directory before calling CopyIn.
  cd [file normalize "$staging_area"]
  foreach file $import_files {
     CopyIn "$file" "$working_area" $copy_option
  }
  set iProjHelper 0
   # Bail if a project currently open.
   if {[IsProjectOpen]} {
      ERR "The project must be closed before performing this operation."
      return 0
   }
   # Must be in the working area (i.e. project directory) before calling recreating the project.
   cd [file normalize "$working_area"]
   INFO "Recreating project \"$project_file\"."
   HandleException {
      set iProjHelper [ OpenFacilProject "$project_file"]
   } "A problem occurred while creating the project \"$project_file\"."
   if {$iProjHelper == 0} {
      cd "$old_working_dir"
      return 0
   }
  set project_settings { 
     "PROP_DevFamily" "Spartan3"
     "PROP_DevDevice" "xc3s200"
     "PROP_DevPackage" "tq144"
     "PROP_DevSpeed" "-4"
     "PROP_Top_Level_Module_Type" "HDL"
     "PROP_Synthesis_Tool" "XST (VHDL/Verilog)"
     "PROP_Simulator" "ISE Simulator (VHDL/Verilog)"
     "PROP_PreferredLanguage" "VHDL"
     "PROP_Enable_Message_Capture" "true"
     "PROP_Enable_Message_Filtering" "false"
     "PROP_Enable_Incremental_Messaging" "false"
     }

  HandleException {
    RestoreProjectSettings $iProjHelper $project_settings 
  } "A problem occured while restoring project settings."

  set user_files { 
      "5i20.ucf" 
      "CountPinsInRange.vhd" 
      "IDROMConst.vhd" 
      "MaxPinsPerModule.vhd" 
      "NumberOfModules.vhd" 
      "PIN_24XQCTRONLY_72.vhd" 
      "PIN_2X7I65_72.vhd" 
      "PIN_ST12_72.vhd" 
      "PIN_SV12IM_2X7I48_72.vhd" 
      "PIN_SV12_72.vhd" 
      "PIN_SV16_96.vhd" 
      "PIN_SV8NA.vhd" 
      "PIN_SV8_48.vhd" 
      "PIN_SVSP4_6_7I46NA_48.vhd" 
      "PIN_SVSP6_2_48.vhd" 
      "PIN_SVSP8_6_7I46_72.vhd" 
      "PIN_SVSPD6_2_48.vhd" 
      "PIN_SVST2_4_7I47_48.vhd" 
      "PIN_SVST2_4_7I47_72.vhd" 
      "PIN_SVST2_8_72.vhd" 
      "PIN_SVST4_12NA_48.vhd" 
      "PIN_SVST4_12_48.vhd" 
      "PIN_SVST4_4NA_48.vhd" 
      "PIN_SVST4_4_48.vhd" 
      "PIN_SVST4_6NA_48.vhd" 
      "PIN_SVST4_6_48.vhd" 
      "PIN_SVST4_8_72.vhd" 
      "PIN_SVST8_24_96.vhd" 
      "PIN_SVST8_4IM2_72.vhd" 
      "PIN_SVST8_4_72.vhd" 
      "PIN_SVST8_8IM2_72.vhd" 
      "PIN_SVST8_8_96.vhd" 
      "PIN_SVSTSP8_12_6_96.vhd" 
      "PIN_SVUA8_4_72.vhd" 
      "PIN_SVUA8_8_72.vhd" 
      "Top9030HostMot2.vhd" 
      "atrans.vhd" 
      "boutreg.vhd" 
      "bufferedspi.vhd" 
      "d8o8.vhd" 
      "dpll.vhd" 
      "hostmot2.vhd" 
      "hostmotid.vhd" 
      "i20card.vhd" 
      "i22_1000card.vhd" 
      "i22_1500card.vhd" 
      "i23card.vhd" 
      "i43_200card.vhd" 
      "i43_400card.vhd" 
      "i65card.vhd" 
      "i68card.vhd" 
      "idrom.vhd" 
      "irqlogic.vhd" 
      "kubstepgenz.vhd" 
      "pwmpdmgenh.vhd" 
      "pwmrefh.vhd" 
      "qcounterate.vhd" 
      "qcountersf.vhd" 
      "simplespi8.vhd" 
      "simplespix.vhd" 
      "simplessi.vhd" 
      "testram.vhd" 
      "testrom.vhd" 
      "timestamp.vhd" 
      "uartr.vhd" 
      "uartr8.vhd" 
      "uartx.vhd" 
      "uartx8.vhd" 
      "ubrategen.vhd" 
      "usbram.vhd" 
      "usbrom.vhd" 
      "watchdog.vhd" 
      "wordpr.vhd" 
      "wordrb.vhd"}

  HandleException {
    AddUserFiles $iProjHelper $user_files
  } "A problem occured while restoring user files."

  set imported_files { 
      "b32test_guide.ncd"}

  set origination 2

  HandleException {
    AddImportedFiles $iProjHelper $imported_files $origination
  } "A problem occured while restoring imported files."

  set process_props { 
      "A" "" "" "" "PROPEXT_SynthMultStyle_virtex2" "Auto" 
      "A" "" "" "" "PROPEXT_xilxBitgCfg_DCIUpdateMode_spartan3" "As Required" 
      "A" "" "" "" "PROPEXT_xilxBitgCfg_Rate_spartan3" "Default (6)" 
      "A" "" "" "" "PROPEXT_xilxMapGenInputK_virtex2" "4" 
      "A" "" "" "" "PROPEXT_xilxSynthAddBufg_spartan3" "8" 
      "A" "" "" "" "PROPEXT_xilxSynthMaxFanout_virtex2" "500" 
      "A" "" "" "" "PROP_CPLDFitkeepio" "false" 
      "A" "" "" "" "PROP_CompxlibAbelLib" "true" 
      "A" "" "" "" "PROP_CompxlibCPLDDetLib" "true" 
      "A" "" "" "" "PROP_CompxlibOtherCompxlibOpts" "" 
      "A" "" "" "" "PROP_CompxlibOutputDir" "$XILINX/<language>/<simulator>" 
      "A" "" "" "" "PROP_CompxlibOverwriteLib" "Overwrite" 
      "A" "" "" "" "PROP_CompxlibSimPrimatives" "true" 
      "A" "" "" "" "PROP_CompxlibXlnxCoreLib" "true" 
      "A" "" "" "" "PROP_CurrentFloorplanFile" "" 
      "A" "" "" "" "PROP_DesignName" "hostmot2" 
      "A" "" "" "" "PROP_Dummy" "dum1" 
      "A" "" "" "" "PROP_EnableWYSIWYG" "None" 
      "A" "" "" "" "PROP_Enable_Incremental_Messaging" "false" 
      "A" "" "" "" "PROP_Enable_Message_Capture" "true" 
      "A" "" "" "" "PROP_Enable_Message_Filtering" "false" 
      "A" "" "" "" "PROP_FitterReportFormat" "Text" 
      "A" "" "" "" "PROP_FlowDebugLevel" "0" 
      "A" "" "" "" "PROP_FunctionBlockInputLimit" "38" 
      "A" "" "" "" "PROP_ISimLibSearchOrderFile" "" 
      "A" "" "" "" "PROP_ISimSDFTimingToBeRead" "Setup Time" 
      "A" "" "" "" "PROP_ISimUseCustomCompilationOrder" "false" 
      "A" "" "" "" "PROP_ISimUseCustomSimCmdFile_behav_tb" "false" 
      "A" "" "" "" "PROP_ISimUseCustomSimCmdFile_behav_tbw" "false" 
      "A" "" "" "" "PROP_ISimUseCustomSimCmdFile_gen_tbw" "false" 
      "A" "" "" "" "PROP_ISimUseCustomSimCmdFile_par_tb" "false" 
      "A" "" "" "" "PROP_ISimUseCustomSimCmdFile_par_tbw" "false" 
      "A" "" "" "" "PROP_ISimUutInstName" "UUT" 
      "A" "" "" "" "PROP_ImpactProjectFile" "" 
      "A" "" "" "" "PROP_Parse_Target" "synthesis" 
      "A" "" "" "" "PROP_PartitionCreateDelete" "" 
      "A" "" "" "" "PROP_PartitionForcePlacement" "" 
      "A" "" "" "" "PROP_PartitionForceSynth" "" 
      "A" "" "" "" "PROP_PartitionForceTranslate" "" 
      "A" "" "" "" "PROP_PlsClockEnable" "true" 
      "A" "" "" "" "PROP_PostTrceFastPath" "false" 
      "A" "" "" "" "PROP_PreTrceFastPath" "false" 
      "A" "" "" "" "PROP_SimModelGenerateTestbenchFile" "false" 
      "A" "" "" "" "PROP_SimModelInsertBuffersPulseSwallow" "false" 
      "A" "" "" "" "PROP_SimModelOtherNetgenOpts" "" 
      "A" "" "" "" "PROP_SimModelRetainHierarchy" "true" 
      "A" "" "" "" "PROP_SynthCaseImplStyle" "None" 
      "A" "" "" "" "PROP_SynthDecoderExtract" "true" 
      "A" "" "" "" "PROP_SynthEncoderExtract" "Yes" 
      "A" "" "" "" "PROP_SynthExtractMux" "Yes" 
      "A" "" "" "" "PROP_SynthExtractRAM" "true" 
      "A" "" "" "" "PROP_SynthExtractROM" "true" 
      "A" "" "" "" "PROP_SynthFsmEncode" "Auto" 
      "A" "" "" "" "PROP_SynthLogicalShifterExtract" "true" 
      "A" "" "" "" "PROP_SynthMultStyle" "LUT" 
      "A" "" "" "" "PROP_SynthOpt" "Area" 
      "A" "" "" "" "PROP_SynthOptEffort" "Normal" 
      "A" "" "" "" "PROP_SynthResSharing" "true" 
      "A" "" "" "" "PROP_SynthShiftRegExtract" "true" 
      "A" "" "" "" "PROP_SynthXORCollapse" "true" 
      "A" "" "" "" "PROP_Top_Level_Module_Type" "HDL" 
      "A" "" "" "" "PROP_UseDataGate" "true" 
      "A" "" "" "" "PROP_UserConstraintEditorPreference" "Constraints Editor" 
      "A" "" "" "" "PROP_UserEditorCustomSetting" "" 
      "A" "" "" "" "PROP_UserEditorPreference" "ISE Text Editor" 
      "A" "" "" "" "PROP_XPowerOptInputTclScript" "" 
      "A" "" "" "" "PROP_XPowerOptLoadPCFFile" "Default" 
      "A" "" "" "" "PROP_XPowerOptLoadVCDFile" "Default" 
      "A" "" "" "" "PROP_XPowerOptLoadXMLFile" "Default" 
      "A" "" "" "" "PROP_XPowerOptOutputFile" "Default" 
      "A" "" "" "" "PROP_XPowerOptVerboseRpt" "false" 
      "A" "" "" "" "PROP_XPowerOtherXPowerOpts" "" 
      "A" "" "" "" "PROP_XplorerMode" "Off" 
      "A" "" "" "" "PROP_bitgen_otherCmdLineOptions" "" 
      "A" "" "" "" "PROP_cpldBestFit" "false" 
      "A" "" "" "" "PROP_cpldfitHDLeqStyle" "Source" 
      "A" "" "" "" "PROP_cpldfit_otherCmdLineOptions" "" 
      "A" "" "" "" "PROP_fitGenSimModel" "false" 
      "A" "" "" "" "PROP_hprep6_autosig" "false" 
      "A" "" "" "" "PROP_hprep6_otherCmdLineOptions" "" 
      "A" "" "" "" "PROP_ibiswriterShowAllModels" "false" 
      "A" "" "" "" "PROP_impactConfigFileName_CPLD" "" 
      "A" "" "" "" "PROP_mapUseRLOCConstraints" "true" 
      "A" "" "" "" "PROP_map_otherCmdLineOptions" "" 
      "A" "" "" "" "PROP_mpprRsltToCopy" "" 
      "A" "" "" "" "PROP_mpprViewPadRptsForAllRslt" "true" 
      "A" "" "" "" "PROP_mpprViewParRptsForAllRslt" "true" 
      "A" "" "" "" "PROP_ngdbuildUseLOCConstraints" "true" 
      "A" "" "" "" "PROP_ngdbuild_otherCmdLineOptions" "" 
      "A" "" "" "" "PROP_parTimingAnalyzerLoadDesign" "true" 
      "A" "" "" "" "PROP_parUseTimingConstraints" "true" 
      "A" "" "" "" "PROP_par_otherCmdLineOptions" "" 
      "A" "" "" "" "PROP_primeCorrelateOutput" "false" 
      "A" "" "" "" "PROP_primeFlatternOutputNetlist" "false" 
      "A" "" "" "" "PROP_primeTopLevelModule" "" 
      "A" "" "" "" "PROP_primetimeBlockRamData" "" 
      "A" "" "" "" "PROP_taengine_otherCmdLineOptions" "" 
      "A" "" "" "" "PROP_xcpldFitDesInit" "Low" 
      "A" "" "" "" "PROP_xcpldFitDesInputLmt_xbr" "32" 
      "A" "" "" "" "PROP_xcpldFitDesMultiLogicOpt" "true" 
      "A" "" "" "" "PROP_xcpldFitDesSlew" "Fast" 
      "A" "" "" "" "PROP_xcpldFitDesTimingCst" "true" 
      "A" "" "" "" "PROP_xcpldFitDesTriMode" "Keeper" 
      "A" "" "" "" "PROP_xcpldFitDesUnused" "Keeper" 
      "A" "" "" "" "PROP_xcpldFitDesVolt" "LVCMOS18" 
      "A" "" "" "" "PROP_xcpldFitTemplate_xpla3" "Optimize Density" 
      "A" "" "" "" "PROP_xcpldFittimRptOption" "Summary" 
      "A" "" "" "" "PROP_xcpldUseGlobalClocks" "true" 
      "A" "" "" "" "PROP_xcpldUseGlobalOutputEnables" "true" 
      "A" "" "" "" "PROP_xcpldUseGlobalSetReset" "true" 
      "A" "" "" "" "PROP_xcpldUseLocConst" "Always" 
      "A" "" "" "" "PROP_xilxBitgCfg_Clk" "Pull Up" 
      "A" "" "" "" "PROP_xilxBitgCfg_Code" "0xFFFFFFFF" 
      "A" "" "" "" "PROP_xilxBitgCfg_DCMShutdown" "false" 
      "A" "" "" "" "PROP_xilxBitgCfg_Done" "Pull Up" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_ASCIIFile" "false" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_BinaryFile" "false" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_BitFile" "true" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_Compress" "false" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_DRC" "true" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_EnableCRC" "true" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_GClkDel0" "11111" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_GClkDel1" "11111" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_GClkDel2" "11111" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_GClkDel3" "11111" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_IEEE1532File" "false" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_IEEE1532File_xbr" "false" 
      "A" "" "" "" "PROP_xilxBitgCfg_GenOpt_ReadBack" "false" 
      "A" "" "" "" "PROP_xilxBitgCfg_M0" "Pull Up" 
      "A" "" "" "" "PROP_xilxBitgCfg_M1" "Pull Up" 
      "A" "" "" "" "PROP_xilxBitgCfg_M2" "Pull Up" 
      "A" "" "" "" "PROP_xilxBitgCfg_Pgm" "Pull Up" 
      "A" "" "" "" "PROP_xilxBitgCfg_Rate" "4" 
      "A" "" "" "" "PROP_xilxBitgCfg_TCK" "Pull Up" 
      "A" "" "" "" "PROP_xilxBitgCfg_TDI" "Pull Up" 
      "A" "" "" "" "PROP_xilxBitgCfg_TDO" "Pull Up" 
      "A" "" "" "" "PROP_xilxBitgCfg_TMS" "Pull Up" 
      "A" "" "" "" "PROP_xilxBitgCfg_Unused" "Pull Down" 
      "A" "" "" "" "PROP_xilxBitgReadBk_Sec" "Enable Readback and Reconfiguration" 
      "A" "" "" "" "PROP_xilxBitgStart_Clk" "CCLK" 
      "A" "" "" "" "PROP_xilxBitgStart_Clk_Done" "6" 
      "A" "" "" "" "PROP_xilxBitgStart_Clk_DriveDone" "false" 
      "A" "" "" "" "PROP_xilxBitgStart_Clk_EnOut" "Default (5)" 
      "A" "" "" "" "PROP_xilxBitgStart_Clk_MatchCycle" "Auto" 
      "A" "" "" "" "PROP_xilxBitgStart_Clk_RelDLL" "Default (NoWait)" 
      "A" "" "" "" "PROP_xilxBitgStart_Clk_RelSet" "Default (6)" 
      "A" "" "" "" "PROP_xilxBitgStart_Clk_WrtEn" "4" 
      "A" "" "" "" "PROP_xilxBitgStart_IntDone" "false" 
      "A" "" "" "" "PROP_xilxMapAllowLogicOpt" "false" 
      "A" "" "" "" "PROP_xilxMapCoverMode" "Speed" 
      "A" "" "" "" "PROP_xilxMapDisableRegOrdering" "false" 
      "A" "" "" "" "PROP_xilxMapGenInputK" "4" 
      "A" "" "" "" "PROP_xilxMapPackRegInto" "For Inputs and Outputs" 
      "A" "" "" "" "PROP_xilxMapReplicateLogic" "true" 
      "A" "" "" "" "PROP_xilxMapReportDetail" "true" 
      "A" "" "" "" "PROP_xilxMapSliceLogicInUnusedBRAMs" "false" 
      "A" "" "" "" "PROP_xilxMapTimingDrivenPacking" "true" 
      "A" "" "" "" "PROP_xilxMapTrimUnconnSig" "true" 
      "A" "" "" "" "PROP_xilxNgdbldIOPads" "false" 
      "A" "" "" "" "PROP_xilxNgdbldMacro" "" 
      "A" "" "" "" "PROP_xilxNgdbldNTType" "Timestamp" 
      "A" "" "" "" "PROP_xilxNgdbldPresHierarchy" "false" 
      "A" "" "" "" "PROP_xilxNgdbldUR" "" 
      "A" "" "" "" "PROP_xilxNgdbldUnexpBlks" "false" 
      "A" "" "" "" "PROP_xilxNgdbld_AUL" "false" 
      "A" "" "" "" "PROP_xilxPARplacerCostTable" "1" 
      "A" "" "" "" "PROP_xilxPARplacerEffortLevel" "None" 
      "A" "" "" "" "PROP_xilxPARrouterEffortLevel" "None" 
      "A" "" "" "" "PROP_xilxPARstrat" "Normal Place and Route" 
      "A" "" "" "" "PROP_xilxPARuseBondedIO" "false" 
      "A" "" "" "" "PROP_xilxPostTrceAdvAna" "false" 
      "A" "" "" "" "PROP_xilxPostTrceRpt" "Error Report" 
      "A" "" "" "" "PROP_xilxPostTrceRptLimit" "500" 
      "A" "" "" "" "PROP_xilxPostTrceStamp" "" 
      "A" "" "" "" "PROP_xilxPostTrceTSIFile" "" 
      "A" "" "" "" "PROP_xilxPostTrceUncovPath" "" 
      "A" "" "" "" "PROP_xilxPreTrceAdvAna" "true" 
      "A" "" "" "" "PROP_xilxPreTrceRpt" "Error Report" 
      "A" "" "" "" "PROP_xilxPreTrceRptLimit" "196" 
      "A" "" "" "" "PROP_xilxPreTrceUncovPath" "" 
      "A" "" "" "" "PROP_xilxSynthAddBufg" "4" 
      "A" "" "" "" "PROP_xilxSynthAddIObuf" "true" 
      "A" "" "" "" "PROP_xilxSynthGlobOpt" "AllClockNets" 
      "A" "" "" "" "PROP_xilxSynthKeepHierarchy" "No" 
      "A" "" "" "" "PROP_xilxSynthKeepHierarchy_CPLD" "Yes" 
      "A" "" "" "" "PROP_xilxSynthMacroPreserve" "true" 
      "A" "" "" "" "PROP_xilxSynthMaxFanout" "100" 
      "A" "" "" "" "PROP_xilxSynthRegBalancing" "No" 
      "A" "" "" "" "PROP_xilxSynthRegDuplication" "true" 
      "A" "" "" "" "PROP_xilxSynthXORPreserve" "true" 
      "A" "" "" "" "PROP_xilxTriStateBuffTXMode" "Off" 
      "A" "" "" "" "PROP_xstAsynToSync" "false" 
      "A" "" "" "" "PROP_xstAutoBRAMPacking" "false" 
      "A" "" "" "" "PROP_xstBRAMUtilRatio" "100" 
      "A" "" "" "" "PROP_xstBusDelimiter" "<>" 
      "A" "" "" "" "PROP_xstCase" "Maintain" 
      "A" "" "" "" "PROP_xstCoresSearchDir" "" 
      "A" "" "" "" "PROP_xstCrossClockAnalysis" "false" 
      "A" "" "" "" "PROP_xstEquivRegRemoval" "true" 
      "A" "" "" "" "PROP_xstFsmStyle" "LUT" 
      "A" "" "" "" "PROP_xstGenerateRTLNetlist" "Yes" 
      "A" "" "" "" "PROP_xstGenericsParameters" "" 
      "A" "" "" "" "PROP_xstHierarchySeparator" "/" 
      "A" "" "" "" "PROP_xstIniFile" "" 
      "A" "" "" "" "PROP_xstLibSearchOrder" "" 
      "A" "" "" "" "PROP_xstOptimizeInsPrimtives" "false" 
      "A" "" "" "" "PROP_xstPackIORegister" "Auto" 
      "A" "" "" "" "PROP_xstReadCores" "true" 
      "A" "" "" "" "PROP_xstSlicePacking" "true" 
      "A" "" "" "" "PROP_xstSliceUtilRatio" "100" 
      "A" "" "" "" "PROP_xstTristate2Logic" "Yes" 
      "A" "" "" "" "PROP_xstUseClockEnable" "Yes" 
      "A" "" "" "" "PROP_xstUseSyncReset" "Yes" 
      "A" "" "" "" "PROP_xstUseSyncSet" "Yes" 
      "A" "" "" "" "PROP_xstUseSynthConstFile" "true" 
      "A" "" "" "" "PROP_xstUserCompileList" "" 
      "A" "" "" "" "PROP_xstVeriIncludeDir_Global" "" 
      "A" "" "" "" "PROP_xstVerilog2001" "true" 
      "A" "" "" "" "PROP_xstVerilogMacros" "" 
      "A" "" "" "" "PROP_xstWorkDir" "./xst" 
      "A" "" "" "" "PROP_xstWriteTimingConstraints" "false" 
      "A" "" "" "" "PROP_xst_otherCmdLineOptions" "" 
      "A" "AutoGeneratedView" "VIEW_AbstractSimulation" "" "PROP_TopDesignUnit" "Architecture|Top9030HostMot2|dataflow" 
      "A" "AutoGeneratedView" "VIEW_AnalyzedDesign" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_AnnotatedPreSimulation" "" "PROP_ISimIncreCompilation" "true" 
      "A" "AutoGeneratedView" "VIEW_AnnotatedPreSimulation" "" "PROP_ISimSpecifyDefMacroAndValue" "" 
      "A" "AutoGeneratedView" "VIEW_AnnotatedPreSimulation" "" "PROP_ISimSpecifySearchDirectory" "" 
      "A" "AutoGeneratedView" "VIEW_AnnotatedPreSimulation" "" "PROP_ISimValueRangeCheck" "false" 
      "A" "AutoGeneratedView" "VIEW_AnnotatedPreSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_AnnotatedResultsFuse" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_AnnotatedResultsISim" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_BehavioralFuse" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_BehavioralSimulationISim" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_FPGAConfiguration" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_FPGAConfigureDevice" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_FPGAGeneratePROM" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Map" "" "PROP_SmartGuide" "false" 
      "A" "AutoGeneratedView" "VIEW_Map" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Par" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Post-MapAbstractSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Post-MapPreSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Post-ParAbstractSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Post-ParFuse" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Post-ParPreSimulation" "" "PROP_ISimCompileForHdlDebug" "true" 
      "A" "AutoGeneratedView" "VIEW_Post-ParPreSimulation" "" "PROP_ISimIncreCompilation" "true" 
      "A" "AutoGeneratedView" "VIEW_Post-ParPreSimulation" "" "PROP_ISimSpecifyDefMacroAndValue" "" 
      "A" "AutoGeneratedView" "VIEW_Post-ParPreSimulation" "" "PROP_ISimSpecifySearchDirectory" "" 
      "A" "AutoGeneratedView" "VIEW_Post-ParPreSimulation" "" "PROP_ISimValueRangeCheck" "false" 
      "A" "AutoGeneratedView" "VIEW_Post-ParPreSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Post-ParSimulationISim" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Post-SynthesisAbstractSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Post-TranslateAbstractSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Post-TranslatePreSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_PostAbstractSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_PreSimulation" "" "PROP_ISimCompileForHdlDebug" "true" 
      "A" "AutoGeneratedView" "VIEW_PreSimulation" "" "PROP_ISimIncreCompilation" "true" 
      "A" "AutoGeneratedView" "VIEW_PreSimulation" "" "PROP_ISimSpecifyDefMacroAndValue" "" 
      "A" "AutoGeneratedView" "VIEW_PreSimulation" "" "PROP_ISimSpecifySearchDir" "" 
      "A" "AutoGeneratedView" "VIEW_PreSimulation" "" "PROP_ISimSpecifySearchDirectory" "" 
      "A" "AutoGeneratedView" "VIEW_PreSimulation" "" "PROP_ISimValueRangeCheck" "false" 
      "A" "AutoGeneratedView" "VIEW_PreSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Structural" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_TBWBehavioralFuse" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_TBWBehavioralSimulationISim" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_TBWPost-MapPreSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_TBWPost-ParFuse" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_TBWPost-ParPreSimulation" "" "PROP_ISimCompileForHdlDebug" "true" 
      "A" "AutoGeneratedView" "VIEW_TBWPost-ParPreSimulation" "" "PROP_ISimIncreCompilation" "true" 
      "A" "AutoGeneratedView" "VIEW_TBWPost-ParPreSimulation" "" "PROP_ISimSpecifyDefMacroAndValue" "" 
      "A" "AutoGeneratedView" "VIEW_TBWPost-ParPreSimulation" "" "PROP_ISimSpecifySearchDirectory" "" 
      "A" "AutoGeneratedView" "VIEW_TBWPost-ParPreSimulation" "" "PROP_ISimValueRangeCheck" "false" 
      "A" "AutoGeneratedView" "VIEW_TBWPost-ParPreSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_TBWPost-ParSimulationISim" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_TBWPost-TranslatePreSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_TBWPreSimulation" "" "PROP_ISimCompileForHdlDebug" "true" 
      "A" "AutoGeneratedView" "VIEW_TBWPreSimulation" "" "PROP_ISimIncreCompilation" "true" 
      "A" "AutoGeneratedView" "VIEW_TBWPreSimulation" "" "PROP_ISimSpecifyDefMacroAndValue" "" 
      "A" "AutoGeneratedView" "VIEW_TBWPreSimulation" "" "PROP_ISimSpecifySearchDirectory" "" 
      "A" "AutoGeneratedView" "VIEW_TBWPreSimulation" "" "PROP_ISimValueRangeCheck" "false" 
      "A" "AutoGeneratedView" "VIEW_TBWPreSimulation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_Translation" "" "PROP_SmartGuide" "false" 
      "A" "AutoGeneratedView" "VIEW_Translation" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_UpdatedBitstream" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_XSTAbstractSynthesis" "" "PROP_SmartGuide" "false" 
      "A" "AutoGeneratedView" "VIEW_XSTAbstractSynthesis" "" "PROP_TopDesignUnit" "Architecture|Top9030HostMot2|dataflow" 
      "A" "AutoGeneratedView" "VIEW_XSTPreSynthesis" "" "PROP_TopDesignUnit" "" 
      "A" "AutoGeneratedView" "VIEW_XSTPreSynthesis" "" "PROP_xstVeriIncludeDir" "" 
      "A" "VIEW_Initial" "VIEW_Initial" "" "PROP_TopDesignUnit" "Architecture|Top9030HostMot2|dataflow" 
      "B" "" "" "" "PROP_AutoGenFile" "false" 
      "B" "" "" "" "PROP_DevFamily" "Spartan3" 
      "B" "" "" "" "PROP_FitterOptimization_xpla3" "Density" 
      "B" "" "" "" "PROP_ISimCustomCompilationOrderFile" "" 
      "B" "" "" "" "PROP_ISimCustomSimCmdFileName_behav_tb" "" 
      "B" "" "" "" "PROP_ISimCustomSimCmdFileName_behav_tbw" "" 
      "B" "" "" "" "PROP_ISimCustomSimCmdFileName_gen_tbw" "" 
      "B" "" "" "" "PROP_ISimCustomSimCmdFileName_par_tb" "" 
      "B" "" "" "" "PROP_ISimCustomSimCmdFileName_par_tbw" "" 
      "B" "" "" "" "PROP_ISimGenVCDFile_par_tb" "false" 
      "B" "" "" "" "PROP_ISimGenVCDFile_par_tbw" "false" 
      "B" "" "" "" "PROP_ISimSimulationRun_behav_tb" "true" 
      "B" "" "" "" "PROP_ISimSimulationRun_behav_tbw" "true" 
      "B" "" "" "" "PROP_ISimSimulationRun_par_tb" "true" 
      "B" "" "" "" "PROP_ISimSimulationRun_par_tbw" "true" 
      "B" "" "" "" "PROP_ISimStoreAllSignalTransitions_behav_tb" "false" 
      "B" "" "" "" "PROP_ISimStoreAllSignalTransitions_behav_tbw" "false" 
      "B" "" "" "" "PROP_ISimStoreAllSignalTransitions_par_tb" "false" 
      "B" "" "" "" "PROP_ISimStoreAllSignalTransitions_par_tbw" "false" 
      "B" "" "" "" "PROP_MapEffortLevel" "Medium" 
      "B" "" "" "" "PROP_MapLogicOptimization" "false" 
      "B" "" "" "" "PROP_MapPlacerCostTable" "1" 
      "B" "" "" "" "PROP_MapPowerReduction" "false" 
      "B" "" "" "" "PROP_MapRegDuplication" "false" 
      "B" "" "" "" "PROP_SimModelRenTopLevInstTo" "UUT" 
      "B" "" "" "" "PROP_Simulator" "ISE Simulator (VHDL/Verilog)" 
      "B" "" "" "" "PROP_SynthConstraintsFile" "" 
      "B" "" "" "" "PROP_SynthMuxStyle" "Auto" 
      "B" "" "" "" "PROP_SynthRAMStyle" "Auto" 
      "B" "" "" "" "PROP_XPowerOptAdvancedVerboseRpt" "false" 
      "B" "" "" "" "PROP_XPowerOptMaxNumberLines" "1000" 
      "B" "" "" "" "PROP_XPowerOptUseTimeBased" "false" 
      "B" "" "" "" "PROP_XplorerEnableRetiming" "true" 
      "B" "" "" "" "PROP_XplorerNumIterations" "7" 
      "B" "" "" "" "PROP_XplorerOtherCmdLineOptions" "" 
      "B" "" "" "" "PROP_XplorerRunType" "Yes" 
      "B" "" "" "" "PROP_XplorerSearchPathForSource" "" 
      "B" "" "" "" "PROP_impactBaud" "None" 
      "B" "" "" "" "PROP_impactConfigMode" "None" 
      "B" "" "" "" "PROP_impactPort" "None" 
      "B" "" "" "" "PROP_mpprViewPadRptForSelRslt" "" 
      "B" "" "" "" "PROP_mpprViewParRptForSelRslt" "" 
      "B" "" "" "" "PROP_parGenAsyDlyRpt" "false" 
      "B" "" "" "" "PROP_parGenClkRegionRpt" "false" 
      "B" "" "" "" "PROP_parGenSimModel" "false" 
      "B" "" "" "" "PROP_parGenTimingRpt" "true" 
      "B" "" "" "" "PROP_parMpprNodelistFile" "" 
      "B" "" "" "" "PROP_parMpprParIterations" "3" 
      "B" "" "" "" "PROP_parMpprResultsDirectory" "" 
      "B" "" "" "" "PROP_parMpprResultsToSave" "" 
      "B" "" "" "" "PROP_parPowerReduction" "false" 
      "B" "" "" "" "PROP_xcpldFitDesInReg_xbr" "true" 
      "B" "" "" "" "PROP_xcpldFitDesPtermLmt_xbr" "28" 
      "B" "" "" "" "PROP_xilxBitgCfg_GenOpt_DbgBitStr" "false" 
      "B" "" "" "" "PROP_xilxBitgCfg_GenOpt_LogicAllocFile" "false" 
      "B" "" "" "" "PROP_xilxBitgCfg_GenOpt_MaskFile" "false" 
      "B" "" "" "" "PROP_xilxBitgReadBk_GenBitStr" "false" 
      "B" "" "" "" "PROP_xilxMapPackfactor" "100" 
      "B" "" "" "" "PROP_xilxPAReffortLevel" "Standard" 
      "B" "" "" "" "PROP_xstMoveFirstFfStage" "true" 
      "B" "" "" "" "PROP_xstMoveLastFfStage" "true" 
      "B" "" "" "" "PROP_xstROMStyle" "Auto" 
      "B" "" "" "" "PROP_xstSafeImplement" "No" 
      "B" "AutoGeneratedView" "VIEW_Map" "" "PROP_ParSmartGuideFileName" "" 
      "B" "AutoGeneratedView" "VIEW_Translation" "" "PROP_MapSmartGuideFileName" "" 
      "C" "" "" "" "PROP_AceActiveName" "" 
      "C" "" "" "" "PROP_CompxlibLang" "VHDL" 
      "C" "" "" "" "PROP_CompxlibSimPath" "Search in Path" 
      "C" "" "" "" "PROP_DevDevice" "xc3s200" 
      "C" "" "" "" "PROP_DevFamilyPMName" "spartan3" 
      "C" "" "" "" "PROP_ISimSimulationRunTime_behav_tb" "1000 ns" 
      "C" "" "" "" "PROP_ISimSimulationRunTime_behav_tbw" "1000 ns" 
      "C" "" "" "" "PROP_ISimSimulationRunTime_par_tb" "1000 ns" 
      "C" "" "" "" "PROP_ISimSimulationRunTime_par_tbw" "1000 ns" 
      "C" "" "" "" "PROP_ISimVCDFileName_par_tb" "xpower.vcd" 
      "C" "" "" "" "PROP_ISimVCDFileName_par_tbw" "xpower.vcd" 
      "C" "" "" "" "PROP_MapExtraEffort" "None" 
      "C" "" "" "" "PROP_SimModelGenMultiHierFile" "false" 
      "C" "" "" "" "PROP_XPowerOptBaseTimeUnit" "ps" 
      "C" "" "" "" "PROP_XPowerOptNumberOfUnits" "1" 
      "C" "" "" "" "PROP_impactConfigFileName" "" 
      "C" "" "" "" "PROP_xilxPARextraEffortLevel" "None" 
      "D" "" "" "" "PROP_CompxlibUni9000Lib" "true" 
      "D" "" "" "" "PROP_CompxlibUniSimLib" "true" 
      "D" "" "" "" "PROP_DevPackage" "tq144" 
      "D" "" "" "" "PROP_Synthesis_Tool" "XST (VHDL/Verilog)" 
      "E" "" "" "" "PROP_DevSpeed" "-4" 
      "E" "" "" "" "PROP_PreferredLanguage" "VHDL" 
      "F" "" "" "" "PROP_ChangeDevSpeed" "-4" 
      "F" "" "" "" "PROP_SimModelTarget" "VHDL" 
      "F" "" "" "" "PROP_hdlInstTempTargetLang" "VHDL" 
      "F" "" "" "" "PROP_tbwTestbenchTargetLang" "VHDL" 
      "F" "" "" "" "PROP_xilxPostTrceSpeed" "-4" 
      "F" "" "" "" "PROP_xilxPreTrceSpeed" "-4" 
      "G" "" "" "" "PROP_PostSynthSimModelName" "_synthesis.vhd" 
      "G" "" "" "" "PROP_SimModelAutoInsertGlblModuleInNetlist" "true" 
      "G" "" "" "" "PROP_SimModelGenArchOnly" "false" 
      "G" "" "" "" "PROP_SimModelIncSdfAnnInVerilogFile" "true" 
      "G" "" "" "" "PROP_SimModelIncSimprimInVerilogFile" "false" 
      "G" "" "" "" "PROP_SimModelIncUnisimInVerilogFile" "false" 
      "G" "" "" "" "PROP_SimModelIncUselibDirInVerilogFile" "false" 
      "G" "" "" "" "PROP_SimModelNoEscapeSignal" "false" 
      "G" "" "" "" "PROP_SimModelOutputExtIdent" "false" 
      "G" "" "" "" "PROP_SimModelRenTopLevArchTo" "Structure" 
      "G" "" "" "" "PROP_SimModelRenTopLevMod" "" 
      "G" "AutoGeneratedView" "VIEW_Map" "" "PROP_PostMapSimModelName" "_map.vhd" 
      "G" "AutoGeneratedView" "VIEW_Par" "" "PROP_PostParSimModelName" "_timesim.vhd" 
      "G" "AutoGeneratedView" "VIEW_Post-MapAbstractSimulation" "" "PROP_tbwPostMapTestbenchName" "" 
      "G" "AutoGeneratedView" "VIEW_Post-ParAbstractSimulation" "" "PROP_tbwPostParTestbenchName" "" 
      "G" "AutoGeneratedView" "VIEW_Post-TranslateAbstractSimulation" "" "PROP_tbwPostXlateTestbenchName" "" 
      "G" "AutoGeneratedView" "VIEW_Translation" "" "PROP_PostXlateSimModelName" "_translate.vhd" 
      "H" "" "" "" "PROP_SimModelBringOutGsrNetAsAPort" "false" 
      "H" "" "" "" "PROP_SimModelBringOutGtsNetAsAPort" "false" 
      "H" "" "" "" "PROP_SimModelPathUsedInSdfAnn" "Default" 
      "H" "AutoGeneratedView" "VIEW_Map" "" "PROP_SimModelRenTopLevEntTo" "" 
      "H" "AutoGeneratedView" "VIEW_Par" "" "PROP_SimModelRenTopLevEntTo" "" 
      "H" "AutoGeneratedView" "VIEW_Structural" "" "PROP_SimModelRenTopLevEntTo" "" 
      "H" "AutoGeneratedView" "VIEW_Translation" "" "PROP_SimModelRenTopLevEntTo" "" 
      "I" "" "" "" "PROP_SimModelGsrPortName" "GSR_PORT" 
      "I" "" "" "" "PROP_SimModelGtsPortName" "GTS_PORT" 
      "I" "" "" "" "PROP_SimModelRocPulseWidth" "100" 
      "I" "" "" "" "PROP_SimModelTocPulseWidth" "0"}

  HandleException {
    RestoreProcessProperties $iProjHelper $process_props
  } "A problem occured while restoring process properties."

   # library names and their members
   set libraries {
   }

  HandleException {
    RestoreSourceLibraries $iProjHelper $libraries
  } "A problem occured while restoring source libraries."

   # partition names for recreation
   set partition_names {
   }

  HandleException {
    RestorePartitions $partition_names
  } "A problem occured while restoring partitions."

   set opts_stream [ [Xilinx::Cit::FactoryCreate $::xilinx::Dpm::StreamBufferCompID ]  GetInterface $xilinx::Prjrep::IStreamID ] 
     $opts_stream WriteString "5"
     $opts_stream WriteString "5"
     $opts_stream WriteString "5"
     $opts_stream WriteString "5"
     $opts_stream WriteString "0"
     $opts_stream WriteString "0"
     $opts_stream WriteString "3"
     $opts_stream WriteString "1"
     $opts_stream WriteString "1"
     $opts_stream WriteString "1"
     $opts_stream WriteString "2"
     $opts_stream WriteString "9"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/4i65.ucf"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/4i68.ucf"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/5i22.ucf"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/5i23.ucf"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/7i43.ucf"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/7i43u.ucf"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/Top9054HostMot2.vhd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/TopEPPHostMot2.vhd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/TopUSBHostMot2.vhd"
     $opts_stream WriteString "19"
     $opts_stream WriteString "./xst"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/b32test_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/DumbAss8_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/HostMot2_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/I20HostMot2_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/I22HostMot2_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/i43hostmot2_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/i43usbhm2_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/Top9030HostMot2_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/Top9054HostMot2_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/TopEPPHostMot2_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/uartr_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/uartr16_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/uartr8_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/uartx_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/uartx16_guide.ncd"
     $opts_stream WriteString "C:/ISE9/5i22/hostmot2/uartx8_guide.ncd"
     $opts_stream WriteString "xpower.vcd"
     $opts_stream WriteString "xpower.vcd"
     $opts_stream WriteString "1"
   RestoreSourceControlOptions "$project_file" $opts_stream
   Release $opts_stream
  if { $srcctrl_comp != 0 } {
     set i_prjref [ $srcctrl_comp GetInterface $::xilinx::Dpm::IProjectHelperReferenceID ]
     $i_prjref Set iProjHelper
  } elseif {$iProjHelper != 0} {
     $iProjHelper Close
  }
  Release $iProjHelper
  # return back
  cd $old_working_dir
}

