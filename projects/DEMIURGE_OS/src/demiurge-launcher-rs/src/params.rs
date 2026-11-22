// Parameter-pool launch contract (docs/parameters.md §11.3).
//
// For every engine stage the launcher sets
//   DEMIURGE_PARAM_PORT   the port the engine listens on for `/p <path> <val>`
//   DEMIURGE_POOL_HOST    where the engine sends `/pout`   (default 127.0.0.1)
//   DEMIURGE_POOL_PORT    ... and its port                 (default 9102)
//   DEMIURGE_STAGE        stage name (manifest `stage`, else the program id)
//   DEMIURGE_STAGE_INDEX  position among the chain stages (0-based)
//   DEMIURGE_RESOURCES    the resources/ dir holding params/<lang>/ hooks
// and, for languages that cannot read the environment, extra argv — but only
// when the hook directory exists, so a Pi without resources/ (or an old
// patch) is launched exactly as before.
//
// Port choice: the manifest `port` when ~/demiurge/params/<x>.json names this
// stage (stage == program id, else stage == lang when that lang is unique in
// the session), otherwise DEMIURGE_OSC_BASE + stage index (docs/osc.md).
//
// Everything here is pure (inputs in, plan out) so it is unit-testable; the
// supervisor does the I/O (reading env, manifests) and spawning.

use crate::config::Program;
use std::path::{Path, PathBuf};

pub const DEFAULT_POOL_HOST: &str = "127.0.0.1";
pub const DEFAULT_POOL_PORT: &str = "9102";
pub const DEFAULT_OSC_BASE: u16 = 9000;

#[derive(Clone, Debug, Default, PartialEq)]
pub struct LaunchPlan {
    pub env: Vec<(String, String)>,
    /// Wrapper argv BEFORE the program file (ChucK: the hook must compile first;
    /// Pd: options must precede the patch). The wrappers exec `<engine> ... "$FILE" "$@"`,
    /// so the first pre_arg lands where the engine expects the file and the rest follow.
    pub pre_args: Vec<String>,
    /// Wrapper argv AFTER the program file.
    pub post_args: Vec<String>,
}

#[derive(Clone, Debug)]
pub struct Ctx {
    pub index: usize,
    pub stage: String,
    pub port: u16,
    pub resources: PathBuf,
    pub pool_host: String,
    pub pool_port: String,
}

/// Programs that are not chain stages (implicit helpers) take no index.
pub fn is_chain_stage(p: &Program) -> bool {
    p.lang != "clock" && p.id != "pdclock"
}

/// 0-based index of programs[idx] among chain stages; None for helpers.
pub fn stage_index(programs: &[Program], idx: usize) -> Option<usize> {
    if !is_chain_stage(programs.get(idx)?) { return None; }
    Some(programs[..idx].iter().filter(|p| is_chain_stage(p)).count())
}

/// Extract `"key": "value"` / `"key": 123` from a manifest without a JSON
/// crate. First occurrence only; manifests carry `stage` and `port` at the
/// top level before `params`.
pub fn json_str(text: &str, key: &str) -> Option<String> {
    let rest = value_after(text, key)?;
    let rest = rest.strip_prefix('"')?;
    Some(rest[..rest.find('"')?].to_string())
}

pub fn json_uint(text: &str, key: &str) -> Option<u32> {
    let rest = value_after(text, key)?;
    let end = rest.find(|c: char| !c.is_ascii_digit()).unwrap_or(rest.len());
    rest[..end].parse().ok()
}

fn value_after<'a>(text: &'a str, key: &str) -> Option<&'a str> {
    let pat = format!("\"{key}\"");
    let at = text.find(&pat)? + pat.len();
    let rest = text[at..].trim_start().strip_prefix(':')?;
    Some(rest.trim_start())
}

/// (stage, port) declared by the manifest texts for this program, if any.
/// `manifests` are the raw file contents.
pub fn manifest_for(manifests: &[String], prog: &Program, lang_is_unique: bool) -> Option<(String, Option<u16>)> {
    let pick = |name: &str| {
        manifests.iter().find(|t| json_str(t, "stage").as_deref() == Some(name)).map(|t| {
            (name.to_string(), json_uint(t, "port").and_then(|p| u16::try_from(p).ok()))
        })
    };
    // A stage may also be named "<something>-<lang>" (e.g. "nld-csound" for the six
    // hello_nonlinear_daylight examples, whose program ids are all identical).
    let pick_suffix = |suffix: &str| {
        let suffix = format!("-{suffix}");
        manifests.iter()
            .find(|t| json_str(t, "stage").map_or(false, |s| s.ends_with(&suffix)))
            .and_then(|t| json_str(t, "stage").map(|s| (s, t)))
            .map(|(s, t)| (s, json_uint(t, "port").and_then(|p| u16::try_from(p).ok())))
    };
    pick(&prog.id).or_else(|| if lang_is_unique { pick(&prog.lang).or_else(|| pick_suffix(&prog.lang)) } else { None })
}

/// Flags for `faust2jackconsole`. Always `-midi` (existing builds are unchanged);
/// `-osc` only when a params manifest names this stage (by program id, or the
/// stage "faust"), because only then is something going to talk OSC to it.
pub fn faust_build_flags(manifests: &[String], prog: &Program) -> Vec<&'static str> {
    let mut f = vec!["-midi"];
    if manifest_for(manifests, prog, true).is_some() { f.push("-osc"); }
    f
}

/// Port shell-quoting-free: argv elements are passed verbatim (no shell).
pub fn build_plan(prog: &Program, ctx: &Ctx) -> LaunchPlan {
    let mut plan = LaunchPlan::default();
    let res = ctx.resources.to_string_lossy().to_string();
    plan.env = vec![
        ("DEMIURGE_PARAM_PORT".into(), ctx.port.to_string()),
        ("DEMIURGE_POOL_HOST".into(), ctx.pool_host.clone()),
        ("DEMIURGE_POOL_PORT".into(), ctx.pool_port.clone()),
        ("DEMIURGE_STAGE".into(), ctx.stage.clone()),
        ("DEMIURGE_STAGE_INDEX".into(), ctx.index.to_string()),
        ("DEMIURGE_RESOURCES".into(), res.clone()),
    ];
    let hook = |sub: &str| ctx.resources.join("params").join(sub);
    match prog.lang.as_str() {
        "csound" if hook("csound").is_dir() => {
            // Csound has no getenv: env -> orchestra macros. The pool host is
            // a *string* macro (the udo pastes it unquoted), so it is passed
            // with literal quotes, and only when it is not the udo's own
            // default (which is already right).
            plan.post_args.push(format!("--omacro:DEMIURGE_PARAM_PORT={}", ctx.port));
            if ctx.pool_host != DEFAULT_POOL_HOST {
                plan.post_args.push(format!("--omacro:DEMIURGE_POOL_HOST=\"{}\"", ctx.pool_host));
            }
            plan.post_args.push(format!("--omacro:DEMIURGE_POOL_PORT={}", ctx.pool_port));
            // so `#include "params/csound/demiurge_params.udo"` resolves
            plan.post_args.push(format!("--env:INCDIR={res}"));
        }
        "chuck" if hook("chuck").join("DemiurgeParams.ck").is_file() => {
            // ChucK compiles files in argv order; the class must exist first.
            plan.pre_args.push(hook("chuck").join("DemiurgeParams.ck").to_string_lossy().to_string());
        }
        "pd" if hook("pd").is_dir() => {
            // Pd has no getenv: -path for the abstractions, -send for the ports.
            // Pd stops parsing options at the first non-option (anything after
            // the patch is treated as a file to open), so these MUST precede
            // the patch: pre_args, which the wrapper splices in front of it.
            plan.pre_args.push("-path".into());
            plan.pre_args.push(hook("pd").to_string_lossy().to_string());
            plan.pre_args.push("-send".into());
            plan.pre_args.push(format!("demiurge-param-port {}; demiurge-pool-port {}", ctx.port, ctx.pool_port));
        }
        // sc, strudel, faust, cpp, python: environment only. sclang takes a
        // single file (the patch loads $DEMIURGE_RESOURCES/params/supercollider/
        // itself), Strudel imports dparams.mjs by URL, Faust/C++ are compiled
        // binaries that read the env directly.
        _ => {}
    }
    plan
}

/// Where the hook library lives: $DEMIURGE_RESOURCES, else <exe dir>/../resources
/// when it exists, else /opt/demiurge/resources.
pub fn resolve_resources(env_value: Option<String>, exe: Option<&Path>) -> PathBuf {
    if let Some(v) = env_value.filter(|v| !v.is_empty()) { return PathBuf::from(v); }
    if let Some(root) = exe.and_then(|e| e.parent()).and_then(|b| b.parent()) {
        let c = root.join("resources");
        if c.is_dir() { return c; }
    }
    PathBuf::from("/opt/demiurge/resources")
}

/// Assemble the context for programs[idx]. `manifests` = raw manifest texts
/// from ~/demiurge/params/*.json; env lookups are injected for testing.
pub fn context_for(
    programs: &[Program],
    idx: usize,
    manifests: &[String],
    getenv: &dyn Fn(&str) -> Option<String>,
    exe: Option<&Path>,
) -> Option<Ctx> {
    let index = stage_index(programs, idx)?;
    let prog = &programs[idx];
    let unique = programs.iter().filter(|p| is_chain_stage(p) && p.lang == prog.lang).count() == 1;
    let man = manifest_for(manifests, prog, unique);
    let base = getenv("DEMIURGE_OSC_BASE").and_then(|s| s.parse::<u16>().ok()).unwrap_or(DEFAULT_OSC_BASE);
    let port = man.as_ref().and_then(|m| m.1).unwrap_or_else(|| base.saturating_add(index as u16));
    let stage = man.map(|m| m.0).unwrap_or_else(|| prog.id.clone());
    Some(Ctx {
        index,
        stage,
        port,
        resources: resolve_resources(getenv("DEMIURGE_RESOURCES"), exe),
        pool_host: getenv("DEMIURGE_POOL_HOST").filter(|s| !s.is_empty()).unwrap_or_else(|| DEFAULT_POOL_HOST.into()),
        pool_port: getenv("DEMIURGE_POOL_PORT").filter(|s| !s.is_empty()).unwrap_or_else(|| DEFAULT_POOL_PORT.into()),
    })
}

/// Read every ~/demiurge/params/*.json (small files; missing dir = none).
pub fn read_manifests(dir: &Path) -> Vec<String> {
    let Ok(rd) = std::fs::read_dir(dir) else { return vec![] };
    let mut v: Vec<_> = rd.flatten().map(|e| e.path())
        .filter(|p| p.extension().map_or(false, |x| x == "json")).collect();
    v.sort();
    v.iter().filter_map(|p| std::fs::read_to_string(p).ok()).collect()
}

/// Full plan for programs[idx] using the real environment. None for helpers.
pub fn plan_for(programs: &[Program], idx: usize) -> Option<LaunchPlan> {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".into());
    let manifests = read_manifests(&Path::new(&home).join("demiurge/params"));
    let exe = std::env::current_exe().ok();
    let ctx = context_for(programs, idx, &manifests, &|k| std::env::var(k).ok(), exe.as_deref())?;
    Some(build_plan(&programs[idx], &ctx))
}

#[cfg(test)]
mod tests {
    #[test]
    fn faust_flags_osc_only_with_manifest() {
        let p = prog("delay", "faust");
        assert_eq!(faust_build_flags(&[], &p), vec!["-midi"]);
        let other = r#"{"stage":"chuck","port":9001,"params":[]}"#.to_string();
        assert_eq!(faust_build_flags(&[other.clone()], &p), vec!["-midi"]);
        let mine = r#"{"stage":"delay","port":9002,"params":[]}"#.to_string();
        assert_eq!(faust_build_flags(&[other.clone(), mine], &p), vec!["-midi", "-osc"]);
        let by_lang = r#"{"stage":"faust","port":9003}"#.to_string();
        assert_eq!(faust_build_flags(&[by_lang], &p), vec!["-midi", "-osc"]);
        let suffixed = r#"{"stage":"nld-faust","port":9000}"#.to_string();
        assert_eq!(faust_build_flags(&[suffixed.clone()], &p), vec!["-midi", "-osc"]);
        assert_eq!(manifest_for(&[suffixed], &prog("hello_nonlinear_daylight", "faust"), true),
                   Some(("nld-faust".to_string(), Some(9000))));
    }

    use super::*;
    use std::fs;

    fn prog(id: &str, lang: &str) -> Program {
        Program { id: id.into(), lang: lang.into(), file: format!("/x/{id}"), midi_only: false }
    }
    fn session() -> Vec<Program> {
        vec![prog("clock", "clock"), prog("pdclock", "python"), prog("a", "strudel"),
             prog("chuck_synth", "chuck"), prog("pd_gate", "pd"), prog("b", "csound"), prog("c", "csound")]
    }
    fn res_dir(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("dem-params-test-{tag}-{}", std::process::id()));
        for l in ["csound", "chuck", "pd"] { fs::create_dir_all(d.join("params").join(l)).unwrap(); }
        fs::write(d.join("params/chuck/DemiurgeParams.ck"), "").unwrap();
        d
    }
    fn ctx(res: &Path, port: u16) -> Ctx {
        Ctx { index: 3, stage: "s".into(), port, resources: res.into(),
              pool_host: "127.0.0.1".into(), pool_port: "9102".into() }
    }
    fn env_of(plan: &LaunchPlan, k: &str) -> Option<String> {
        plan.env.iter().find(|(a, _)| a == k).map(|(_, v)| v.clone())
    }

    #[test]
    fn index_skips_helpers() {
        let s = session();
        assert_eq!(stage_index(&s, 0), None);
        assert_eq!(stage_index(&s, 1), None);
        assert_eq!(stage_index(&s, 2), Some(0));
        assert_eq!(stage_index(&s, 4), Some(2));
    }

    #[test]
    fn json_scan() {
        let t = r#"{ "stage": "chuck", "port": 9001, "version": 1, "params": [{"path":"/chuck/x"}] }"#;
        assert_eq!(json_str(t, "stage").as_deref(), Some("chuck"));
        assert_eq!(json_uint(t, "port"), Some(9001));
        assert_eq!(json_uint(t, "nope"), None);
    }

    #[test]
    fn port_manifest_by_id_then_unique_lang_else_base_plus_index() {
        let s = session();
        let m = vec![r#"{"stage":"chuck","port":9042}"#.to_string(), r#"{"stage":"csound","port":9050}"#.to_string()];
        let none: &dyn Fn(&str) -> Option<String> = &|_| None;
        // unique lang chuck -> manifest port + stage name
        let c = context_for(&s, 3, &m, none, None).unwrap();
        assert_eq!((c.port, c.stage.as_str()), (9042, "chuck"));
        // pd has no manifest: base + index 2, stage = id
        let c = context_for(&s, 4, &m, none, None).unwrap();
        assert_eq!((c.port, c.stage.as_str()), (9002, "pd_gate"));
        // csound appears twice: lang match refused, base + index
        let c = context_for(&s, 5, &m, none, None).unwrap();
        assert_eq!((c.port, c.stage.as_str()), (9003, "b"));
        // id match always wins
        let m2 = vec![r#"{"stage":"c","port":9099}"#.to_string()];
        assert_eq!(context_for(&s, 6, &m2, none, None).unwrap().port, 9099);
        // helpers get nothing
        assert!(context_for(&s, 0, &m, none, None).is_none());
    }

    #[test]
    fn osc_base_and_env_overrides() {
        let s = session();
        let env = |k: &str| match k {
            "DEMIURGE_OSC_BASE" => Some("9500".to_string()),
            "DEMIURGE_RESOURCES" => Some("/r".to_string()),
            "DEMIURGE_POOL_HOST" => Some("10.0.0.2".to_string()),
            "DEMIURGE_POOL_PORT" => Some("9111".to_string()),
            _ => None,
        };
        let c = context_for(&s, 2, &[], &env, None).unwrap();
        assert_eq!((c.port, c.resources.to_str().unwrap(), c.pool_host.as_str(), c.pool_port.as_str()),
                   (9500, "/r", "10.0.0.2", "9111"));
    }

    #[test]
    fn resources_default_chain() {
        assert_eq!(resolve_resources(Some("/e".into()), None), PathBuf::from("/e"));
        assert_eq!(resolve_resources(Some("".into()), None), PathBuf::from("/opt/demiurge/resources"));
        let d = res_dir("exe");
        let bin = d.join("bin"); fs::create_dir_all(&bin).unwrap();
        // d has no `resources` dir yet -> falls through
        assert_eq!(resolve_resources(None, Some(&bin.join("demiurge-launcher"))), PathBuf::from("/opt/demiurge/resources"));
        fs::create_dir_all(d.join("resources")).unwrap();
        assert_eq!(resolve_resources(None, Some(&bin.join("demiurge-launcher"))), d.join("resources"));
        fs::remove_dir_all(d).ok();
    }

    #[test]
    fn env_always_set() {
        let d = res_dir("env");
        let p = build_plan(&prog("x", "faust"), &ctx(&d, 9004));
        assert_eq!(env_of(&p, "DEMIURGE_PARAM_PORT").as_deref(), Some("9004"));
        assert_eq!(env_of(&p, "DEMIURGE_POOL_PORT").as_deref(), Some("9102"));
        assert_eq!(env_of(&p, "DEMIURGE_STAGE").as_deref(), Some("s"));
        assert_eq!(env_of(&p, "DEMIURGE_STAGE_INDEX").as_deref(), Some("3"));
        assert_eq!(env_of(&p, "DEMIURGE_RESOURCES"), Some(d.to_string_lossy().to_string()));
        assert!(p.pre_args.is_empty() && p.post_args.is_empty()); // env-only language
        fs::remove_dir_all(d).ok();
    }

    #[test]
    fn csound_argv() {
        let d = res_dir("cs");
        let p = build_plan(&prog("x", "csound"), &ctx(&d, 9006));
        assert_eq!(p.post_args, vec![
            "--omacro:DEMIURGE_PARAM_PORT=9006".to_string(),
            "--omacro:DEMIURGE_POOL_PORT=9102".to_string(),
            format!("--env:INCDIR={}", d.display()),
        ]);
        // non-default host is passed as a quoted string macro
        let mut c = ctx(&d, 9006); c.pool_host = "10.0.0.2".into();
        let p = build_plan(&prog("x", "csound"), &c);
        assert!(p.post_args.contains(&"--omacro:DEMIURGE_POOL_HOST=\"10.0.0.2\"".to_string()));
        fs::remove_dir_all(d).ok();
    }

    #[test]
    fn chuck_hook_goes_before_the_file() {
        let d = res_dir("ck");
        let p = build_plan(&prog("x", "chuck"), &ctx(&d, 9001));
        assert_eq!(p.pre_args, vec![d.join("params/chuck/DemiurgeParams.ck").to_string_lossy().to_string()]);
        assert!(p.post_args.is_empty());
        fs::remove_dir_all(d).ok();
    }

    #[test]
    fn pd_argv() {
        let d = res_dir("pd");
        let p = build_plan(&prog("x", "pd"), &ctx(&d, 9002));
        assert!(p.post_args.is_empty());
        assert_eq!(p.pre_args, vec![
            "-path".to_string(), d.join("params/pd").to_string_lossy().to_string(),
            "-send".to_string(), "demiurge-param-port 9002; demiurge-pool-port 9102".to_string(),
        ]);
        fs::remove_dir_all(d).ok();
    }

    #[test]
    fn no_hook_dir_no_argv_old_patches_unaffected() {
        let d = std::env::temp_dir().join(format!("dem-params-test-empty-{}", std::process::id()));
        fs::create_dir_all(&d).unwrap();
        for l in ["csound", "chuck", "pd", "sc", "strudel", "faust", "cpp"] {
            let p = build_plan(&prog("x", l), &ctx(&d, 9000));
            assert!(p.pre_args.is_empty() && p.post_args.is_empty(), "{l}");
            assert_eq!(p.env.len(), 6);
        }
        fs::remove_dir_all(d).ok();
    }
}
