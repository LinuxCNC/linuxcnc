package cgen

import (
	"bytes"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

func TestGenerateClientGoSimple(t *testing.T) {
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
					{Name: "opt_desc", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string", Nullable: true}},
				},
			},
		},
		Funcs: []ast.Func{
			{
				Name:   "list_items",
				Method: "GET",
				Path:   "/items",
				Params: []ast.Param{
					{Name: "pattern", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string", Nullable: true}},
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
	err := GenerateClientGo(&buf, api, "testclient")
	if err != nil {
		t.Fatalf("GenerateClientGo: %v", err)
	}

	out := buf.String()

	// Check package
	assertContains(t, out, "package testclient")

	// Check imports
	assertContains(t, out, `"encoding/json"`)
	assertContains(t, out, `"net/http"`)
	assertContains(t, out, `"net/url"`)

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
	assertContains(t, out, `OptDesc *string `+"`"+`json:"opt_desc,omitempty"`+"`")

	// Check APIError type
	assertContains(t, out, "type APIError struct {")
	assertContains(t, out, "StatusCode int")
	assertContains(t, out, "Message    string")

	// Check client struct
	assertContains(t, out, "type TestapiClient struct {")
	assertContains(t, out, "baseURL string")
	assertContains(t, out, "http    *http.Client")

	// Check constructor
	assertContains(t, out, "func NewTestapiClient(baseURL string) *TestapiClient")
	assertContains(t, out, `"/api/v1/testapi"`)

	// Check WithHTTPClient
	assertContains(t, out, "func (c *TestapiClient) WithHTTPClient(hc *http.Client) *TestapiClient")

	// Check doRequest helper
	assertContains(t, out, "func (c *TestapiClient) doRequest(method, path string, body interface{}, result interface{}) error")

	// Check method signatures
	assertContains(t, out, "func (c *TestapiClient) ListItems(pattern *string) ([]Item, error)")
	assertContains(t, out, "func (c *TestapiClient) GetItem(name string) (*Item, error)")
	assertContains(t, out, "func (c *TestapiClient) CreateItem(name string, value float64) (*Item, error)")
	assertContains(t, out, "func (c *TestapiClient) DeleteItem(name string) error")

	// Check path parameter substitution
	assertContains(t, out, `path = strings.Replace(path, "{name}", url.PathEscape(name), 1)`)

	// Check query parameter handling for nullable
	assertContains(t, out, "if pattern != nil {")
	assertContains(t, out, `query.Set("pattern", *pattern)`)

	// Check POST body handling
	assertContains(t, out, `Name string `+"`"+`json:"name"`+"`")
	assertContains(t, out, `Value float64 `+"`"+`json:"value"`+"`")

	// Check HTTP method usage
	assertContains(t, out, `c.doRequest("GET", path,`)
	assertContains(t, out, `c.doRequest("POST", path,`)
	assertContains(t, out, `c.doRequest("DELETE", path,`)
}

func TestGenerateClientGoPathParams(t *testing.T) {
	api := &ast.API{
		Name:       "paths",
		Version:    1,
		Prefix:     "paths",
		RestExport: true,
		Funcs: []ast.Func{
			{
				Name:   "get_nested",
				Method: "GET",
				Path:   "/parent/{parentId}/child/{childId}",
				Params: []ast.Param{
					{Name: "parentId", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
					{Name: "childId", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
				},
				Return: &ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"},
			},
		},
	}

	var buf bytes.Buffer
	err := GenerateClientGo(&buf, api, "pathsclient")
	if err != nil {
		t.Fatalf("GenerateClientGo: %v", err)
	}

	out := buf.String()

	// Check multiple path params are substituted
	assertContains(t, out, `path = strings.Replace(path, "{parentId}", url.PathEscape(parentId), 1)`)
	assertContains(t, out, `path = strings.Replace(path, "{childId}", url.PathEscape(childId), 1)`)
}

func TestGenerateClientGoNoREST(t *testing.T) {
	api := &ast.API{
		Name:       "internal",
		Version:    1,
		Prefix:     "internal",
		RestExport: false, // Not REST exported
		Funcs: []ast.Func{
			{
				Name: "do_stuff",
				Params: []ast.Param{
					{Name: "value", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32"}},
				},
			},
		},
	}

	var buf bytes.Buffer
	err := GenerateClientGo(&buf, api, "internalclient")
	if err != nil {
		t.Fatalf("GenerateClientGo: %v", err)
	}

	out := buf.String()

	// Should still generate - REST client can call non-REST APIs if server exposes them
	assertContains(t, out, "type InternalClient struct {")
	assertContains(t, out, "func (c *InternalClient) DoStuff(value int32) error")
}

func TestExtractPathParams(t *testing.T) {
	tests := []struct {
		path     string
		expected []string
	}{
		{"/items", nil},
		{"/item/{name}", []string{"name"}},
		{"/parent/{parentId}/child/{childId}", []string{"parentId", "childId"}},
		{"/a/{x}/b/{y}/c/{z}", []string{"x", "y", "z"}},
		{"/{only}", []string{"only"}},
	}

	for _, tc := range tests {
		result := extractPathParams(tc.path)
		if len(result) != len(tc.expected) {
			t.Errorf("extractPathParams(%q): got %v, want %v", tc.path, result, tc.expected)
			continue
		}
		for i, p := range result {
			if p != tc.expected[i] {
				t.Errorf("extractPathParams(%q)[%d]: got %q, want %q", tc.path, i, p, tc.expected[i])
			}
		}
	}
}
