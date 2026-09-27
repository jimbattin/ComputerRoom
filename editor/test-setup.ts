// `bun test` preload (bunfig.toml): happy-dom's window, document and localStorage as globals.
import { GlobalRegistrator } from '@happy-dom/global-registrator';

GlobalRegistrator.register();
