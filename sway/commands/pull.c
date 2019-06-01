#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/commands.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"
#include "stringop.h"
#include "list.h"
#include "log.h"
#include "util.h"

static const char expected_syntax[] =
	"Expected 'pull workspace <name>' or "
	"'pull output <name>'";

/*
 * Get the active workspace of an output, and assert that it is not NULL.
 */
static struct sway_workspace *output_assert_workspace(
		struct sway_output *output) {
	struct sway_workspace *ws = output_get_active_workspace(output);
	sway_assert(ws, "Expected output to have a workspace");
	return ws;
}

struct cmd_results *cmd_pull(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "pull workspace",
				EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	// Output and active workspace we're pulling too
	struct sway_seat *seat = config->handler_context.seat;
	struct sway_workspace *workspace = config->handler_context.workspace;
	struct sway_output *output = workspace->output;
	if (!workspace) {
		return cmd_results_new(CMD_FAILURE, "pull needs an active workspace");
	}

	// The output and workspace being pulled.
	struct sway_output *other_output = NULL;
	struct sway_workspace *other_workspace = NULL;

	// Pull workspace by name.
	if (strcasecmp(argv[0], "workspace") == 0) {
		other_workspace = workspace_by_name(argv[1]);
		if (!other_workspace) {
			other_workspace = workspace_create(NULL, argv[1]);
		}
		other_output = other_workspace->output;

	// Pull workspace from an output.
	} else if (strcasecmp(argv[0], "output") == 0) {
		other_output = output_by_name_or_id(argv[1]);
		if (!other_output) {
			return cmd_results_new(CMD_FAILURE, "unknown output");
		}
		other_workspace = output_assert_workspace(other_output);
		if (!other_workspace) {
			return cmd_results_new(CMD_FAILURE,
				"Expected output to have a workspace");
		}

	// Pull what now?
	} else {
		return cmd_results_new(CMD_INVALID, expected_syntax);
	}

	// Pulling the current workspace would be a NOP.
	if (workspace == other_workspace) {
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	struct sway_node *focus = seat_get_focus_inactive(seat,
			&other_workspace->node);

	// Do the switcheroo.
	// We swap the workspaces if pulling from another output,
	// that way we never have to create or destroy workspaces.
	if (output != other_output) {
		output_add_workspace(output, other_workspace);
		workspace_output_raise_priority(other_workspace, other_output, output);
		if (other_output != NULL) {
			output_add_workspace(other_output, workspace);
			workspace_output_raise_priority(workspace, output, other_output);
		}
		ipc_event_workspace(NULL, other_workspace, "move");
		if (other_output != NULL) {
			ipc_event_workspace(NULL, workspace, "move");
		}
		arrange_output(output);
		if (other_output != NULL) {
			arrange_output(other_output);
		}
	}

	seat_set_focus(seat, focus);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
