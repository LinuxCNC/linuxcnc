import emccanon


def report(self, *args):
    """Report the pockets as Python sees them, and flag any disagreement with
    the named parameters."""
    py = (self.current_pocket, self.selected_pocket)
    np = (int(self.params["_current_pocket"]), int(self.params["_selected_pocket"]))
    emccanon.MESSAGE("py: current_pocket=%d selected_pocket=%d%s"
                     % (py + ("" if py == np else
                              " MISMATCH: #<_current_pocket>=%d #<_selected_pocket>=%d" % np,)))
    return 0.0
