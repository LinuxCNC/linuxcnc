package cgen

import (
	"bytes"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

func TestGenerateServerGoSimple(t *testing.T) {
	api := &ast.API{
		Name:       "testapi",
		Version:    1,
		Prefix:     "testapi",
		RestExport: true,
		Enums: []ast.Enum{
			{
				Name: "Color",
				Values: []ast.EnumValue{
					{Name: "RED", Value: 0},
					{Name: "GREEN", Value: 1},
					{Name: "BLUE", Value: 2},
				},
			},
		},
		Types: []ast.Type{
			{
				Name: "Item",
				Fields: []ast.Field{
					{Name: "name", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
					{Name: "value", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "f64"}},
					{Name: "color", Type: ast.TypeRef{Kind: ast.TypeNamed, Name: "Color"}},
					{Name: "tags", Type: ast.TypeRef{Kind: ast.TypeSlice, Elem: &ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}}},
					{Name: "opt_count", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32", Nullable: true}},
				},
			},
		},
		Funcs: []ast.Func{
			{
				Name:   "list_items",
				Method: "GET",
				Path:   "/items",
				Params: []ast.Param{
					{Name: "pattern", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
				},
				Return: &ast.TypeRef{Kind: ast.TypeSlice, Elem: &ast.TypeRef{Kind: ast.TypeNamed, Name: "Item"}},
			},
			{
				Name:   "get_item",
				Method: "GET",
				Path:   "/item/{name}",
				Params: []ast.Param{
					{Name: "name", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
				},
				Return: &ast.TypeRef{Kind: ast.TypeNamed, Name: "Item"},
			},
			{
				Name:   "create_item",
				Method: "POST",
				Path:   "/item",
				Params: []ast.Param{
					{Name: "name", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
					{Name: "value", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "f64"}},
				},
				Return: &ast.TypeRef{Kind: ast.TypeNamed, Name: "Item"},
			},
			{
				Name:   "delete_item",
				Method: "DELETE",
				Path:   "/item/{name}",
				Params: []ast.Param{
					{Name: "name", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
				},
			},
		},
	}

	var buf bytes.Buffer
	err := GenerateServerGo(&buf, api, "testpkg")
	if err != nil {
		t.Fatalf("GenerateServerGo: %v", err)
	}

	out := buf.String()

	// Check package
	assertContains(t, out, "package testpkg")

	// Check imports
	assertContains(t, out, `"encoding/json"`)
	assertContains(t, out, `"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"`)

	// Check enum
	assertContains(t, out, "type Color int32")
	assertContains(t, out, "RED Color = 0")
	assertContains(t, out, "GREEN Color = 1")
	assertContains(t, out, "BLUE Color = 2")

	// Check struct
	assertContains(t, out, "type Item struct {")
	assertContains(t, out, `Name string `+"`"+`json:"name"`+"`")
	assertContains(t, out, `Value float64 `+"`"+`json:"value"`+"`")
	assertContains(t, out, `Color Color `+"`"+`json:"color"`+"`")
	assertContains(t, out, `Tags []string `+"`"+`json:"tags"`+"`")
	assertContains(t, out, `OptCount *int32 `+"`"+`json:"opt_count,omitempty"`+"`")

	// Check callbacks interface
	assertContains(t, out, "type TestapiCallbacks interface {")
	assertContains(t, out, "ListItems(pattern string) ([]Item, error)")
	assertContains(t, out, "GetItem(name string) (*Item, error)")
	assertContains(t, out, "CreateItem(name string, value float64) (*Item, error)")
	assertContains(t, out, "DeleteItem(name string) error")

	// Check dispatch functions exist
	assertContains(t, out, "func testapiDispatchListItems(callbacks unsafe.Pointer, req []byte) ([]byte, error)")
	assertContains(t, out, "func testapiDispatchGetItem(callbacks unsafe.Pointer, req []byte) ([]byte, error)")
	assertContains(t, out, "func testapiDispatchCreateItem(callbacks unsafe.Pointer, req []byte) ([]byte, error)")
	assertContains(t, out, "func testapiDispatchDeleteItem(callbacks unsafe.Pointer, req []byte) ([]byte, error)")

	// Check JSON unmarshal in dispatch
	assertContains(t, out, `Pattern string `+"`"+`json:"pattern"`+"`")

	// Check void return dispatch
	assertContains(t, out, "impl.DeleteItem(params.Name)")
	assertContains(t, out, "return nil, nil") // void return

	// Check APIMeta
	assertContains(t, out, `var TestapiMeta = &apiserver.APIMeta{`)
	assertContains(t, out, `Name:       "testapi"`)
	assertContains(t, out, `Version:    1`)
	assertContains(t, out, `RESTExport: true`)
	assertContains(t, out, `Prefix:     "testapi"`)
	assertContains(t, out, `Name:     "list_items"`)
	assertContains(t, out, `Method:   "GET"`)
	assertContains(t, out, `Path:     "/items"`)
	assertContains(t, out, `Dispatch: testapiDispatchListItems`)

	// Check Register function
	assertContains(t, out, "func RegisterTestapiAPI(registry *apiserver.Registry, instance string, impl TestapiCallbacks) error")
	assertContains(t, out, `registry.Register("testapi", 1, instance, unsafe.Pointer(&cb))`)
}

func TestGenerateServerGoKeywordEscape(t *testing.T) {
	api := &ast.API{
		Name:       "myapi",
		Version:    1,
		RestExport: true,
		Funcs: []ast.Func{
			{
				Name:   "new_type",
				Method: "POST",
				Path:   "/type",
				Params: []ast.Param{
					{Name: "type", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
					{Name: "func", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
				},
				Return: &ast.TypeRef{Kind: ast.TypePrimitive, Name: "bool"},
			},
		},
	}

	var buf bytes.Buffer
	err := GenerateServerGo(&buf, api, "testpkg")
	if err != nil {
		t.Fatal(err)
	}

	out := buf.String()

	// Interface params should have escaped keywords
	assertContains(t, out, "NewType(type_ string, func_ string) (bool, error)")

	// Params struct uses PascalCase (no collision)
	assertContains(t, out, `Type string `+"`"+`json:"type"`+"`")
	assertContains(t, out, `Func string `+"`"+`json:"func"`+"`")

	// Call passes PascalCase struct fields
	assertContains(t, out, "impl.NewType(params.Type, params.Func)")
}

func TestGenerateServerGoNoREST(t *testing.T) {
	api := &ast.API{
		Name:       "internal",
		Version:    2,
		RestExport: false,
		Funcs: []ast.Func{
			{
				Name:   "do_work",
				RTSafe: true,
				Params: []ast.Param{
					{Name: "id", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32"}},
				},
			},
		},
	}

	var buf bytes.Buffer
	err := GenerateServerGo(&buf, api, "testpkg")
	if err != nil {
		t.Fatal(err)
	}

	out := buf.String()

	// Should still generate everything
	assertContains(t, out, "type InternalCallbacks interface")
	assertContains(t, out, "DoWork(id int32) error")
	assertContains(t, out, `RESTExport: false`)
	// Non-REST functions get dispatch generated but are NOT in FuncMeta
	assertContains(t, out, "func internalDispatchDoWork(callbacks unsafe.Pointer, req []byte) ([]byte, error)")
}

func assertContains(t *testing.T, s, substr string) {
	t.Helper()
	if !strings.Contains(s, substr) {
		t.Errorf("output missing expected substring:\n  want: %q\n  in output of %d bytes", substr, len(s))
	}
}
