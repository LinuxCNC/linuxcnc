package classicladder

import "testing"

func TestSFC_InitStep(t *testing.T) {
	rt := newTestRT()
	defer freeTestRT(rt)

	// Set up step 0 as init step on page 0
	rt.steps[0].num_page = 0
	rt.steps[0].init_step = 1
	rt.steps[0].step_number = 0

	// Set up step 1 (not init) on page 0
	rt.steps[1].num_page = 0
	rt.steps[1].init_step = 0
	rt.steps[1].step_number = 1

	testPrepareSequential(rt)

	if rt.steps[0].activated != 1 {
		t.Error("init step should be activated after prepare")
	}
	if rt.steps[1].activated != 0 {
		t.Error("non-init step should not be activated")
	}
}

func TestSFC_TransitionFires(t *testing.T) {
	rt := newTestRT()
	defer freeTestRT(rt)

	// Step 0: init step on page 0
	rt.steps[0].num_page = 0
	rt.steps[0].init_step = 1
	rt.steps[0].step_number = 0

	// Step 1: on page 0
	rt.steps[1].num_page = 0
	rt.steps[1].init_step = 0
	rt.steps[1].step_number = 1

	// Transition 0: condition = MEM_BIT[0], deactivates step 0, activates step 1
	rt.transitions[0].num_page = 0
	rt.transitions[0].var_type_condi = 0 // CL_VAR_MEM_BIT
	rt.transitions[0].var_num_condi = 0
	rt.transitions[0].num_step_to_desactiv[0] = 0
	rt.transitions[0].num_step_to_desactiv[1] = -1
	rt.transitions[0].num_step_to_activ[0] = 1
	rt.transitions[0].num_step_to_activ[1] = -1

	testPrepareSequential(rt)

	// Condition not yet true
	rt.var_bits[0] = 0
	testRefreshSequentialPage(rt, 0)

	if rt.steps[0].activated != 1 {
		t.Error("step 0 should still be active (condition false)")
	}
	if rt.steps[1].activated != 0 {
		t.Error("step 1 should not be active yet")
	}

	// Set condition true
	rt.var_bits[0] = 1
	testRefreshSequentialPage(rt, 0)

	if rt.steps[0].activated != 0 {
		t.Error("step 0 should be deactivated after transition fires")
	}
	if rt.steps[1].activated != 1 {
		t.Error("step 1 should be activated after transition fires")
	}
}

func TestSFC_ANDDivergence(t *testing.T) {
	rt := newTestRT()
	defer freeTestRT(rt)

	// Step 0: init step
	rt.steps[0].num_page = 0
	rt.steps[0].init_step = 1
	rt.steps[0].step_number = 0

	// Steps 1 and 2: parallel branches
	rt.steps[1].num_page = 0
	rt.steps[1].step_number = 1
	rt.steps[2].num_page = 0
	rt.steps[2].step_number = 2

	// Transition: deactivates step 0, activates steps 1 AND 2
	rt.transitions[0].num_page = 0
	rt.transitions[0].var_type_condi = 0
	rt.transitions[0].var_num_condi = 0
	rt.transitions[0].num_step_to_desactiv[0] = 0
	rt.transitions[0].num_step_to_desactiv[1] = -1
	rt.transitions[0].num_step_to_activ[0] = 1
	rt.transitions[0].num_step_to_activ[1] = 2
	rt.transitions[0].num_step_to_activ[2] = -1

	testPrepareSequential(rt)
	rt.var_bits[0] = 1
	testRefreshSequentialPage(rt, 0)

	if rt.steps[0].activated != 0 {
		t.Error("step 0 should be deactivated")
	}
	if rt.steps[1].activated != 1 {
		t.Error("step 1 should be activated (AND divergence)")
	}
	if rt.steps[2].activated != 1 {
		t.Error("step 2 should be activated (AND divergence)")
	}
}

func TestSFC_ANDConvergence(t *testing.T) {
	rt := newTestRT()
	defer freeTestRT(rt)

	// Steps 0 and 1: both active (parallel branches converging)
	rt.steps[0].num_page = 0
	rt.steps[0].init_step = 1
	rt.steps[0].step_number = 0
	rt.steps[1].num_page = 0
	rt.steps[1].init_step = 1
	rt.steps[1].step_number = 1

	// Step 2: target after convergence
	rt.steps[2].num_page = 0
	rt.steps[2].step_number = 2

	// Transition requires BOTH step 0 and 1 to be active
	rt.transitions[0].num_page = 0
	rt.transitions[0].var_type_condi = 0
	rt.transitions[0].var_num_condi = 0
	rt.transitions[0].num_step_to_desactiv[0] = 0
	rt.transitions[0].num_step_to_desactiv[1] = 1
	rt.transitions[0].num_step_to_desactiv[2] = -1
	rt.transitions[0].num_step_to_activ[0] = 2
	rt.transitions[0].num_step_to_activ[1] = -1

	testPrepareSequential(rt)
	rt.var_bits[0] = 1
	testRefreshSequentialPage(rt, 0)

	if rt.steps[0].activated != 0 {
		t.Error("step 0 should be deactivated (AND convergence)")
	}
	if rt.steps[1].activated != 0 {
		t.Error("step 1 should be deactivated (AND convergence)")
	}
	if rt.steps[2].activated != 1 {
		t.Error("step 2 should be activated (AND convergence)")
	}
}

func TestSFC_StepTimeAccumulates(t *testing.T) {
	rt := newTestRT()
	defer freeTestRT(rt)

	rt.steps[0].num_page = 0
	rt.steps[0].init_step = 1
	rt.steps[0].step_number = 0
	rt.periodic_refresh_ms = 10

	testPrepareSequential(rt)

	// No transitions, just refresh to accumulate time
	testRefreshSequentialPage(rt, 0)
	testRefreshSequentialPage(rt, 0)
	testRefreshSequentialPage(rt, 0)

	if rt.steps[0].time_activated != 30 {
		t.Errorf("step time = %d, want 30", rt.steps[0].time_activated)
	}
}
