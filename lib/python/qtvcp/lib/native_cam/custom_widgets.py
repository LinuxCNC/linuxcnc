import os

from PyQt5.QtCore import QVariant, pyqtSlot, Qt, QAbstractItemModel, QModelIndex, QSize
from PyQt5.QtWidgets import (QStyledItemDelegate, QComboBox, QWidget, QVBoxLayout,
                    QToolBar, QToolButton,  QLabel, QListWidget, QListWidgetItem,
                    QInputDialog)
from PyQt5.QtGui import QFont, QColor, QIcon
from qtvcp.widgets.calculator import Calculator

HORIZONTAL_HEADERS = ("Property", "Value")

class tv_select :  # 'enum' items
    none, feature, items, header, param = list(range(5))

########################
# Meta class holds the XML, tool list and visibility data.
# calls to methods not in this class are called in Featire and Parameter class
########################
class MetaClass(object):
    '''
    a trivial custom data object
    '''
    def __init__(self, xml, tool_tip, m_visible, is_visible, tool_list = None, options = None):
        self.xml = xml
        self.tool_tip = tool_tip
        self.m_visible = m_visible
        self.is_visible = is_visible
        self.tool_list = tool_list
        self.options = options

    def find_attr(self, name):
        try:
            attr = self.xml.attr[name] if name in self.xml.attr else ''
            return attr
        except:
            return None

    def set_attr(self, name,value):
        try:
            self.xml.attr[name] = value
            return True
        except Exception as e:
            print(e)
            return False

    # call xml (Feature or Parameter) class methods if available
    def __getattr__(self, attr):
        return getattr(self.xml, attr)

    def __repr__(self):
        return "meta Object - %s %s %s"% (self.xml, self.tool_tip, self.m_visible)

#######################
# an item holds the data class and item hierarchy
#######################
class TreeItem(object):
    '''
    a python object used to return row/column data, and keep note of
    it's parents and/or children
    '''
    def __init__(self, parentItem, data, header = 'test' ):
        self.meta = data
        self.parentItem = parentItem
        self.header = header
        self.childItems = []

    def appendChild(self, item):
        self.childItems.append(item)

    def insertChild(self,row, item):
        print('insert',self.childItems,row, len(self.childItems))
        if row > len(self.childItems):
            return False
        if row <0:
            return False

        ret = self.childItems.insert(row,item)
        print(self.childItems, ret)
        return ret

    def child(self, row):
        try:
            return self.childItems[row]
        except:
            return None

    def childCount(self):
        return len(self.childItems)

    def columnCount(self):
        return 2
    
    def data(self, column):
        if self.meta == None:
            if column == 0:
                return QVariant(self.header)
            elif column == 1:
                return QVariant("")                
        else:
            if column == 0:
                try:
                    name = self.meta.get_name()
                    #print('error:',self.meta)
                except Exception as e:
                    print(e,'\nerror:',self.meta)
                    #name = self.meta.xml.attr['name'].lower() if 'name' in self.meta.xml.attr else ''
                    return None
                return QVariant(name)
                #return QVariant(self.meta.tool_tip)
            if column == 1:
                value = self.meta.get_value()
                return QVariant(value)
        return QVariant()

    def delete(self):
        self.parentItem.childItems.remove(self)

    def parent(self):
        return self.parentItem

    def atTop(self):
        print(self.parentItem)
        return bool(self.parentItem == None)

    def nextSibling(self):
        next = self.row() +1
        if next > self.parentItem.childCount()-1:
            return None
        return self.parentItem.child(next)

    def row(self):
        if self.parentItem:
            return self.parentItem.childItems.index(self)
        return 0

    def canMoveUp(self):
        print('up?',self.parentItem.childItems.index(self))
        if self.parentItem.childItems.index(self) == 0:
            return False
        return True

    def canMoveDown(self):
        print('down?',self.parentItem.childItems.index(self) )
        if self.parentItem.childItems.index(self) == childCount()-1:
            return False
        return True

    def findPath(self):
        def look(i,path):
                #print('look item',i, path,i.parentItem )
                if not i.parentItem.meta.find_attr('name') == None:
                    look(i.parentItem, path + str(i.row()))
                return path
        #print('look done',look(self, str(self.row())))
        return look(self, str(self.row()))

    def getFeature(self):
        return self.meta.xml

    def __iter__(self):
        self._iternum = -1
        return self

    def __next__(self):
        self._iternum += 1
        if self._iternum < self.childCount():
            #print('next:',self.child(self._iternum),self._iternum,self.childCount(),self.childItems)
            return self.child(self._iternum)
        else:
            raise StopIteration

    def __repr__(self):
        return "Item Object - %s (%s children)"% (self.meta.find_attr('name'), self.childCount())

##################
# adds a combobox to treeview
##################
class ComboDelegate(QStyledItemDelegate):
    """
    A delegate that places a fully functioning QComboBox in every
    cell of the column to which it's applied
    """
    def __init__(self, parent):
        super(ComboDelegate, self).__init__(parent)

    @staticmethod
    def data(index):
        """
        Custom method to derive the model data from the model.
         
        Note: assumes there is an intermediate proxyModel.
        """
        # get the sourceModel, we know that the index.model() is a proxy model. 
        # If you don't know you can use the following:
        proxy_model = index.model()
        try:
             source_model = proxy_model.sourceModel()
             source_index = proxy_model.mapToSource(index)
        except:
             source_model = proxy_model
             source_index = index
        return source_model.getItemFromIndex(source_index).qt_data(index.column())

    def createEditor(self, parent, option, index):
        editorType = index.model().data(index,Qt.UserRole).get_type() 
        if editorType == 'combo':
            if index.data() =='': return
            combo = QComboBox(parent)
            combo.setFont(QFont("Times New Roman", pointSize = 15, weight=QFont.Bold))

            # add text and user data from XML'options' to combobox
            items = index.model().data(index,Qt.UserRole).get_options().split(':')
            for i in items:
                data = i.split('=') # text, user data ('magic number')
                combo.addItem(data[0],data[1])
            combo.currentIndexChanged.connect(self.currentIndexChanged)
            return combo
        elif editorType == 'tool':
            combo = QComboBox(parent)
            combo.setFont(QFont("Times New Roman", pointSize = 15, weight=QFont.Bold))
            items = index.model().DATA.TOOL_TABLE.list.split(':')
            for i in items:
                data = i.split('=') # text, user data ('magic number')
                combo.addItem(data[0],data[1])
            combo.currentIndexChanged.connect(self.currentIndexChanged)
            return combo
        elif editorType == 'text':
            dialog = QInputDialog(parent)
            dialog.setWindowFlags(dialog.windowFlags() | Qt.Dialog |
                            Qt.WindowStaysOnTopHint | Qt.WindowSystemMenuHint)
            dialog.setOption(QInputDialog.UsePlainTextEditForTextInput)
            dialog.setWindowTitle('Note')
            dialog.setLabelText('Editable Comment (installed in Gcode)')
            return dialog
        elif editorType in ('int','float'):
            calc = Calculator(parent)
            return calc
        else:
            return super().createEditor(parent, option, index)

    
    def setEditorData(self, editor, index):
        editorType = index.model().data(index,Qt.UserRole).get_type()

        # set combo box index from treemodel
        if editorType == 'combo':
            # get model data 'magic number'
            value = index.model().data(index,Qt.UserRole).get_value()
            idx = editor.findData(value) 
            #print('set combo', idx)
            editor.setCurrentIndex(idx)
            return

        elif editorType == 'tool':
            # get model data 'magic number'
            value = index.model().data(index,Qt.UserRole).get_value()
            idx = editor.findData(value) 
            print('set tool combo', idx)
            editor.setCurrentIndex(int(value))
            return

        elif editorType == 'text':
            text = index.model().data(index,Qt.UserRole).get_value()
            editor.setTextValue(text)

        elif editorType in ('int','float'):
            text = index.model().data(index,Qt.UserRole).get_value()
            editor.display.setText(text)

        else:
            return super().setEditorData(editor, index)

    def setModelData(self, editor, model, index):
        editorType = index.model().data(index,Qt.UserRole).get_type()

        # set model to combobox selection
        if editorType == 'combo':
            if index.column() == 1:
                # extract integer user data ('magic number') from combobox
                data = editor.itemData(editor.currentIndex(), Qt.UserRole)
                #print('set model data',data)
                model.setData(index, data, Qt.DisplayRole)

        elif editorType == 'tool':
                # extract integer user data ('magic number') from combobox
                data = editor.itemData(editor.currentIndex(), Qt.UserRole)
                print('set tool model data',data)
                model.setData(index, data, Qt.DisplayRole)

        elif editorType == 'text':
            data = editor.textValue()
            result = editor.result()
            if result == QInputDialog.Accepted:
                model.setData(index, data, Qt.DisplayRole)

        elif editorType in('int','float'):
            if editorType == 'int':
                result = editor.intResult
            else:
                result = editor.floatResult
            if not result is None:
                model.setData(index, str(result), Qt.EditRole)

        else:
            return super().setModelData(editor, model, index)

    @pyqtSlot()
    def currentIndexChanged(self):
        self.commitData.emit(self.sender())
        self.closeEditor.emit(self.sender())

###############
# Tree model to manipulate data
###############
class treeModel(QAbstractItemModel):
    '''
    a model to display a few names, ordered by sex
    '''
    def __init__(self, parent=None):
        super(treeModel, self).__init__(parent)
        self.rootItem = TreeItem(None, MetaClass(None, 'Root Item', False, False), "ROOT")
        self.parents = {None:self.rootItem}
        self.iconBasePath = 'graphics'
        self.DATA = Data()

    def clear(self):
        self.beginResetModel()
        self.rootItem = TreeItem(None, MetaClass(None, 'Root Item', False, False), "ROOT")
        self.parents = {None:self.rootItem}
        self.endResetModel()

    def columnCount(self, parent=None):
        if parent and parent.isValid():
            return parent.internalPointer().columnCount()
        else:
            return len(HORIZONTAL_HEADERS)

    # get data from the treemodel
    def data(self, index, role):
        #print('role',index, role)
        if not index.isValid():
            print('not valid')
            return QVariant()
        item = index.internalPointer()

        if role == Qt.DisplayRole:

            if item.meta and index.column() == 1:
                print('*:',item.meta.get_type())
                # don't display data if there is a checkbox
                if item.meta.get_type() == 'bool':
                    return None

                elif item.meta.get_type() == 'combo':
                    def get_data(): # from XML object
                        return item.data(index.column()).value()
                    optionList = item.meta.get_options().split(':')
                    #print('option list',optionList, get_data())
                    for temp in(optionList):
                        text,num = temp.split('=')
                        #print ('look at ',text,num)
                        if str(num) == str(get_data()):
                            #print('bingo')
                            return text
                    return('Unknown {}'.format(get_data()))

                elif item.meta.get_type() == 'tool':
                    val = item.meta.find_attr('value')
                    text = self.DATA.TOOL_TABLE.get_text(val)
                    print(self.DATA.TOOL_TABLE.list.split(':'))
                    print('tool',val,text)
                    return text

                elif item.meta.get_type() == 'prjname':
                    h, dval = os.path.split(self.DATA.CURRENT_PROJECT)
                    dval, h = os.path.splitext(dval)
                    print('prjname',dval,h)
                    return dval

                elif item.meta.get_type() == 'text':
                    val = item.meta.find_attr('value')
                    print('Found ->note text:',val)
                    return val

                else:
                    print('Unknown data {}'.format(item.meta.get_type()))

            return item.data(index.column())

        elif role == Qt.FontRole:
            if item.childCount() == 0:
                return QFont("Times New Roman", pointSize = 15)
            else:
                return QFont("Times New Roman", pointSize = 15, weight=QFont.Bold)

        elif role == Qt.BackgroundRole and index.column() == 0:
            if item.childCount() > 0:
                return QColor(225, 225, 225)

        elif role == Qt.SizeHintRole:
            return QSize(32,32)

        elif role == Qt.UserRole:
            if item:
                return item.meta

        elif role == Qt.ToolTipRole:
            return item.meta.get_tooltip()

        elif role == Qt.DecorationRole and item.meta and index.column() == 0:
            value =  item.meta.get_icon(16)
            return QIcon('{}/{}'.format(self.iconBasePath, value))

        elif role == Qt.CheckStateRole and item.meta and index.column() == 1:
            if item.meta.find_attr('type') == 'bool':
                if item.meta.find_attr('value') == '1':
                    return Qt.Checked
                else:
                    return Qt.Unchecked

        return None

    def setData(self, index, value, role):
        col = index.column()
        row = index.row()

        if not index.isValid():
            return False
        item = index.internalPointer()

        print('model setData:',value,item.meta.get_type())
        print('=>', item.meta,role)

        if role == Qt.CheckStateRole:
          if item.meta.find_attr('type') in ('bool'):
            if value == Qt.Checked:
                item.meta.set_value('1')
            else:
                print(item.meta.set_value)
                item.meta.set_value('0')
            self.dataChanged.emit(index, index)
            return True

        elif role == Qt.EditRole:
            print('editrole',role,index.column())
            if item.meta and index.column() == 1:
                metaType = item.meta.get_type()
                #print(item.meta.find_attr('type'))

                if metaType  in ('float','int'):
                    print('int/float',item.meta.xml)
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)

                elif metaType == 'tool':
                    print('Set Tool',value)
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)

                elif metaType == 'engrave':
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)

                elif metaType == 'text':
                    print('Set Note text')
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)

                elif metaType == 'prjname':
                    print('prjname')

                else:
                    print('unknown but trying updated:',metaType)
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)

        elif role == Qt.DisplayRole:
            print('display role',role,index.column())
            if item.meta and index.column() == 1:
                metaType = item.meta.get_type()
                if metaType in ('combo', 'combo-user', 'list'):
                    optionList = item.meta.get_options().split(':')
                    #print(optionList,value)
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)
                elif metaType == 'tool':
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)
                elif metaType == 'text':
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)
                elif metaType  in ('float','int'):
                    print('Set int/float')
                    rtn = item.meta.set_value(value)
                    self.dataChanged.emit(index, index)
        print('after=>', item.meta)
        #item.meta.fname = value

        return True

    def get_feature(self,item, column=0):
        if column == 0:
            return item.meta.xml

    # Returns the item flags for the given index.
    def flags(self, index):
        if not index.isValid():
            return None
        if index.column() == 1:
            return Qt.ItemIsEnabled | Qt.ItemIsUserCheckable | Qt.ItemIsSelectable | Qt.ItemIsEditable
        else:
            return Qt.ItemIsEnabled | Qt.ItemIsSelectable

    def headerData(self, column, orientation, role):
        if (orientation == Qt.Horizontal and
        role == Qt.DisplayRole):
            try:
                return QVariant(HORIZONTAL_HEADERS[column])
            except IndexError:
                pass

        return None

    def index(self, row, column, parent):
        if not self.hasIndex(row, column, parent):
            return QModelIndex()

        if not parent.isValid():
            parentItem = self.rootItem
        else:
            parentItem = parent.internalPointer()

        childItem = parentItem.child(row)
        if childItem:
            return self.createIndex(row, column, childItem)
        else:
            return QModelIndex()

    def indexToPath(self, index):
        # creates a gtk style path (0:1:2) from index

        p = (index.parent().parent().row(),index.parent().row(),index.row())
        path =''
        for num,i in enumerate(p):
            if i == -1:
                continue
            path += str(i)
            if num <2:
                path += ':'
        if path =='':
            return None
        return path

    def parent(self, index):
        if not index.isValid():
            return QModelIndex()

        childItem = index.internalPointer()
        if not childItem:
            return QModelIndex()
        
        parentItem = childItem.parent()

        if parentItem == self.rootItem:
            return QModelIndex()

        if parentItem is None:
            return QModelIndex()
        return self.createIndex(parentItem.row(), 0, parentItem)

    def item_parent(self, item):
        return item.parentItem

    def printItemName(self, item):
        try:
            name = item.meta.find_attr('name')
        except:
            return 'None'
        return name

    def rowCount(self, parent=QModelIndex()):
        if parent.column() > 0:
            return 0
        if not parent.isValid():
            p_Item = self.rootItem
        else:
            p_Item = parent.internalPointer()
        return p_Item.childCount()

    def removeRows(self, row, count, parentIndex):
        self.beginRemoveRows(parentIndex,row,row+count)

        parentItem = self.getItemFromIndex(parentIndex)
        for i in range(row, row+count):
            print('remove',i,parentItem.child(i))
            parentItem.child(i).delete()

        self.endRemoveRows()

    def insertItem(self, row, parentItem, item):
        print('insert item',row, item)
        parentItem.insertChild(row, item)
        self.layoutChanged.emit()

    def moveRow(self,item, delta):

        parent = item.parent()
        parentIndex = self.createIndex(parent.row(), 0, parent)
        row = item.row()

        self.removeRows(row,1,parentIndex)
        self.insertItem(row+delta, parent, item )

        return
        # in almost all QTreeView implementations the "tree" structure is at column 0...
        curr_index = self.selectionModel().currentIndex().siblingAtColumn(0)
        if curr_index.isValid():
            curr_item = self.model().itemFromIndex(curr_index)
            curr_row = curr_index.row()
            # if this is the top sibling of its parent it cant be moved up... 
            if curr_row > 0: 
                parent_item = self.model().itemFromIndex(curr_index.parent())
                # NB parent_item is None in the case of root children: 
                # in that case you therefore use "takeRow" and "insertRow" methods of the model, not the item!
                take_row = self.model().takeRow if parent_item == None else parent_item.takeRow 
                insert_row = self.model().insertRow if parent_item == None else parent_item.insertRow 
                row_to_move = take_row(curr_row)
                insert_row(curr_row - 1, row_to_move)
                new_index = self.model().indexFromItem(curr_item)
                # now set the (single) selection, and set current,  back to the moved item 
                # (if you are implementing single-selection, obviously)
                flag = QtCore.QItemSelectionModel.SelectionFlag
                self.selectionModel().setCurrentIndex(new_index, flag.Clear | flag.SelectCurrent)

    def addNode(self, parent, data):
            xml, tool_tip, m_visible, is_visible, toollist = data
            if parent is None:
                parentItem = self.rootItem
            else:
                parentItem = parent
            #print('addNode',parentItem)
            temp = MetaClass(xml, tool_tip, m_visible, is_visible, toollist)
            newItem = TreeItem(parentItem, temp)
            #print('add item',self. printItemName(newItem),'to',self. printItemName(parentItem),parentItem)

            self.layoutAboutToBeChanged.emit()
            parentItem.appendChild(newItem)
            self.layoutChanged.emit()
            return newItem

    def searchModel(self, meta):
        '''
        get the modelIndex for a given appointment
        '''
        def searchNode(node):
            '''
            a function called recursively, looking at all nodes beneath node
            '''
            for child in node.childItems:
                if meta == child.meta:
                    index = self.createIndex(child.row(), 0, child)
                    return index
                    
                if child.childCount() > 0:
                    result = searchNode(child)
                    if result:
                        return result
        
        retarg = searchNode(self.parents[0])
        print (retarg)
        return retarg
            
    def find_GivenName(self, fname):
        app = None
        for meta in self.people:
            if meta.fname == fname:
                app = meta
                break
        if app != None:
            index = self.searchModel(app)
            return (True, index)            
        return (False, None)

    # get the top item
    def get_iter_root(self):
        return self.rootItem

    def getFirstItem(self):
        return self.rootItem.child(0)

    def getFirstChildItem(self, parent):
        return parent.child(0)

    def getNextSibling(self, item):
        '''
        Return the next sibling of the item or None
        '''
        return item.nextSibling()

    def getItemFromIndex(self,index, column = 0):

        if isinstance(index, QModelIndex):
            return index.internalPointer()

        # from list of model indexes:
        if not index == []:
            item = index[column].internalPointer()
            print ('selected: row',index[column].row(), index[column].internalPointer()) 
            return item
        return None

    def forEachParent(self,function):
        for child in self.rootItem.childItems:
            function(child)

    def remove(self, item):
        print('REMOVE!: ',item)
        item.delete()
        self.layoutChanged.emit()

#################
# custom widgets
#################

##### toolbutton with label

class ToolButton(QWidget):
    def __init__(self, text, path_icon, tooltip, src, parent=None):
        super(ToolButton, self).__init__(parent)
        lay = QVBoxLayout(self)
        toolButton = QToolButton()
        if path_icon is not None :
            toolButton.setIcon(QIcon(path_icon))
            toolButton.setIconSize(QSize(64, 64))
        if (tooltip is not None) and (tooltip != '') :
            toolbutton.setToolTip(tooltip)
        if src is not None :
            print('add click')
            toolbutton.clicked.connect(lambda s, src=src: self.add_feature(None, src))

        label = QLabel(text)
        lay.addWidget(toolButton, 0, Qt.AlignCenter)
        lay.addWidget(label, 0, Qt.AlignCenter)
        lay.setContentsMargins(0, 0, 0, 0)

###### Icon View of menu data

class IconView(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent=None)
        self.parent = parent
        self.iconSize = QSize(100,100)
        self.topMenu = None
        self.InitWindow()

    def InitWindow(self):
        vbox = QVBoxLayout()

        self._toolbar = QToolBar()
        self._toolbar.addAction(self.parent.actionWindowClose)
        self._toolbar.addAction(self.parent.actionLoadCfg)
        self._toolbar.addAction(self.parent.actionImportXML)
        self._toolbar.addAction(self.parent.actionBack)
        self.parent.actionBack.triggered.connect(self.back)
        vbox.setMenuBar(self._toolbar)

        self.list = QListWidget()
        self.list.setViewMode(QListWidget.IconMode)
        self.list.setResizeMode(QListWidget.Adjust)
        self.list.setIconSize(QSize(96,96))
        self.list.clicked.connect(self.listview_clicked)
        vbox.addWidget(self.list)
        self.label = QLabel()
        self.label.setFont(QFont("Sanserif", 15))
        vbox.addWidget(self.label)
        self.setLayout(vbox)
        self.show()

#[get_pixbuf(p.get("icon"), add_dlg_icon_size), p.tag.lower(), _(name), src, path, tooltip])
    def buildItem(self,text,icon,tooltip,action=None):
        item = QListWidgetItem()
        item.setText(text)
        item.setIcon(icon)
        item.setSizeHint(self.iconSize)
        item.setToolTip(tooltip)
        item.setFlags(Qt.ItemIsEnabled)
        item.setData(Qt.UserRole, action)
        return item

    def showTopList(self, tlist=None):
        if not tlist is None:
            self.topMenu = tlist
        self.list.clear()
        if self.topMenu is None: return
        for num, i in enumerate(self.topMenu.actions()):
            if i.isSeparator():
                continue
            item = self.buildItem(i.iconText(),i.icon(),i.iconText(),i)
            self.list.insertItem(num, item)
        self.parent.actionBack.setVisible(False)

    def showSubList(self, action):
        #print('sub',action.menu())
        self.list.clear()
        for num, i in enumerate(action.menu().actions()):
            if i.isSeparator():
                continue
            item = self.buildItem(i.iconText(),i.icon(),i.iconText(),i)
            self.list.insertItem(num, item)
        self.parent.actionBack.setVisible(True)

    def listview_clicked(self):
        item = self.list.currentItem()
        #print(item,item.data(Qt.UserRole).menu())
        
        # action has submenu? - no: trigger the action
        if item.data(Qt.UserRole).menu() is None:
            self.label.setText(str(item.text()))
            item.data(Qt.UserRole).trigger()
            self.showTopList()
            self.parent.hideIconView()
        # yes: show next sublevel
        else:
            self.showSubList(item.data(Qt.UserRole))

    def back(self):
        self.showTopList()

class Data(object):
    _instance = None
    current_dir =  os.path.dirname(__file__)

    # directories
    CFG_DIR = 'cfg'
    PROJECTS_DIR = 'projects'
    LIB_DIR = 'lib'
    NGC_DIR = 'scripts'
    EXAMPLES_DIR = 'examples'
    CATALOGS_DIR = 'catalogs'
    GRAPHICS_DIR =  os.path.abspath(os.path.join(current_dir, 'graphics'))
    DEFAULTS_DIR = 'defaults'
    CUSTOM_DIR = 'my-stuff'

    # files
    DEFAULT_TEMPLATE = 'default_template.xml'
    USER_DEFAULT_FILE = 'custom_defaults.conf'
    EXCL_MSG_FILE = 'excluded_msg.conf'
    CURRENT_WORK = "current_work.xml"
    PREFERENCES_FILE = "default.conf"
    CONFIG_FILE = 'ncam.conf'
    TOOLBAR_FNAME = "toolbar.conf"
    TOOLBAR_CUSTOM_FNAME = "toolbar-custom.conf"
    GENERATED_FILE = "ncam.ngc"

    CURRENT_PROJECT = ''

    DEFAULT_EDITOR = 'gedit'

    SUPPORTED_DATA_TYPES = ['sub-header', 'header', 'bool', 'boolean', 'int', 'gc-lines',
                        'tool', 'gcode', 'text', 'list', 'float', 'string', 'engrave',
                        'combo', 'combo-user', 'items', 'filename', 'prjname']
    NUMBER_TYPES = ['float', 'int']
    NO_ICON_TYPES = ['sub-header', 'header']
    GROUP_HEADER_TYPES = ['items', 'sub-header', 'header']

    XML_TAG = "lcnc-ncam"

    HOME_PAGE = 'https://github.com/FernV/NativeCAM'

    def __init__(self):
        # only initialize once for all instances
        if not self.__class__._instance is None:
            return

    def __new__(cls):
        if cls._instance is None:
            print('Creating the object')
            cls._instance = super(Data, cls).__new__(cls)
        return cls._instance


if __name__ == "__main__":
    from PyQt5.QtWidgets import QApplication
    import sys

    App = QApplication(sys.argv)
    window = IconView()
    sys.exit(App.exec())
