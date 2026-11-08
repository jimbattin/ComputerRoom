import { render } from "preact";
import { App } from "./App";

const root = document.getElementById("root");
if (!root) throw new Error("editor.html has no #root");
render(<App />, root);
