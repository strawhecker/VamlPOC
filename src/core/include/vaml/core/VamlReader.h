#pragma once
// *****************************************************************************
// DUAL-PATH CONTRACT (S290, dep:S289) — S2 reader = DEV TOOL, S7 reader = RELEASE
// *****************************************************************************
// This `vaml::VamlReader` is the S2 TEXT reader — the DEV TOOL. Its role:
//   * the live text reload path in vaml_viewer (S222_1),
//   * the S256 viewer binding path (text → bound tree),
//   * vamlcc's text→compiled INPUT (S283: `compile_to_bytes`).
// It consumes VAML TEXT directly (`Load(std::string_view)`).
//
// The RELEASE path is the S7 compiled reader (`vaml::tools::VBamlReader` +
// `ResourcePack`, tools) over compiled `.vbaml` BYTES — the ONLY reader the
// release app-shell links. This reader is NEVER linked by release; the
// compiled-only entry (`VBamlReader::InitializeComponentFrom`, S289) takes a
// pack identity resolving to COMPILED bytes, never text.
//
// The ONLY text→anything transition in the whole tree is `vamlcc`
// (`compile_to_bytes`, S283) — the dev compiler, never linked by release.
// S288 proved behavioral parity; S290 formalized the split as a documented
// contract + refactor-gated audit. See docs/roadmap.md + progress.md.
// *****************************************************************************
//
// S092 (dep:S091) — VamlReader extensions: (1) records the x:Name registry,
// (2) retains deferred markup-extension values (the S2 tag), and (3) exposes a
// MINIMAL REAL event-hook surface (Sprint 8). Everything S091 delivered is
// unchanged; S092 ADDS the strands described below.
//
// The three S092 strands (see the S092 plan):
//   A. x:Name / x:Class registry — build() now CONSUMES the S083 Name/Class
//      directives (previously skipped): x:Name registers the built object
//      under that name (root or child); x:Class records the authored root
//      class name. The binding of that class name to an owner object/type is
//      deliberately deferred to Sprint 8 (S286 / InitializeComponent-analogue)
//      and to S243 — recorded NO-GAP, not silent. A duplicate x:Name with-in a
//      single Load throws VamlReaderException(DuplicateName).
//   B. Deferred markup extensions (the S2 tag) — an S084-classified isExtension
//      attribute ({Binding Title}) is no longer skipped silently; build()
//      records it as a DeferredExtension (target object, attribute key, and the
//      ORIGINAL source text). The reader does NOT resolve the extension — that
//      is S252/S253's real contract (S2 tag: "resolved by S252"). Retained only,
//      but findable via deferred_extensions().
//   C. Event hooks (Sprint 8) — MINIMAL REAL seed. register_event(name) declares
//      that an attribute key is an EVENT, not a property; build() routes a
//      matching declaration to a DeferredEventHandler (target, eventName,
//      handlerName) instead of attempting a property set. The actual routed-
//      event engine is S320/S322; the S092 surface only defers event wiring
//      deterministically.
//
// S091 continues to own everything not listed above (resolve + literal prop
// set + child linking + unknown-property baseline). S094 still owns the full
// reader error taxonomy; S092 introduces exactly the ONE genuinely-new reader
// error it can produce: DuplicateName (validated by this step's tests).

#include "vaml/core/BindingExpression.h" // BindingExpression (the S252 live binding)
#include "vaml/core/Brush.h"          // S460 — GradientStopHost + LinearGradientBrush (BRUSH AUTHORING)
#include "vaml/core/VectorDrawing.h"  // 11k-4 — geometry/drawing/vector-brush resource types
#include "vaml/core/DependencyObject.h" // unique_ptr<DependencyObject>
#include "vaml/core/Style.h"          // S454 — authored <Style>/<Setter>/<Trigger> authoring
#include "vaml/core/StyleApplier.h"   // S454 — apply (explicit/implicit) + find_implicit
#include "vaml/core/DependencyProperty.h" // DependencyProperty (convertLiteral decl)
#include "vaml/core/ICommand.h"        // ICommand (the S253 {Command} resolution target)
#include "vaml/core/ObservableObject.h" // ObservableObject (the S252 data context / VM)
#include "vaml/core/ResourceDictionary.h" // S361 — ResourceDictionary (x:Key capture)
#include "vaml/core/ResourceResolver.h" // S361 — DynamicResourceLink (reader-owned dynamic links)
#include "vaml/core/RoutedEvent.h"     // RoutedEvent / RoutedEventHandler (S322 event wiring)
#include "vaml/core/export.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml {

struct ParsedElement; // forward-declared (Load(const ParsedElement&) takes a ref)

// S091: the reader's OWN error taxonomy — contains ONLY what the reader can
// itself produce (S048 discipline — no dead enum arms). An attribute may name
// a property that has no global DependencyProperty definition; that is this
// step's one genuinely reader-local error. Everything else the reader can hit
// already propagates as a structured error from a lower layer
// (ElementResolutionException / ValueConversionException).
//
// S092 adds ONE arm the reader can now genuinely produce: a duplicate x:Name
// registered within a single Load is a real authoring error (WPF-faithful —
// a name scope with a repeated name is an error, fail loud, no silent
// last-wins). No other arm is invented; S094 still owns the full taxonomy.
//
// S094 (dep:S093) — the reader error-taxonomy CONSOLIDATION GATE. The four
// title cases (unknown attr, unknown element, bad type name, malformed value)
// are all already structured + reachable through one realistic reader path,
// and S094 proves them on that path (VamlReader_s094_test.cpp). It also
// resolves the S092-recorded hand-off: `x:Class` is root-only by definition
// (WPF-faithful), so a Class directive on a NON-root element was being recorded
// as root_class_ silently — a silent last-wins hole. S094 makes it a genuine
// reader error via the ONE new arm below. No other arm is invented (the
// unknown-type / malformed-value genres live in their lower layers and
// propagate as-is; duplicate-typed arms there would be duplicate surface).
enum class VamlReaderError
{
    None,          // no reader error
    UnknownProperty, // an attribute names a property with no global definition (S091)
    DuplicateName, // an x:Name was registered more than once in one Load (S092)
    ClassOnNonRoot, // an x:Class directive was authored on a NON-root element (S094)
    MalformedBindingMarkup, // a {Binding ...} / {Command ...} source with bad inner syntax (S252)
    NoEventHandler // a markup event's handlerName has no registered class-owner handler (S322)
};

// S091: the reader error — names the offending attribute key. what() renders
// "unknown property '<key>'". Parallel to ElementResolutionException (S088) /
// ValueConversionException (S089) at the reader layer, and the S094-documented
// baseline of the full reader error taxonomy.
//
// S092: for DuplicateName, `property`/`name` holds the duplicate x:Name value;
// what() renders "duplicate x:Name '<name>'" (the offending NAME, not the
// attribute key). S094: for ClassOnNonRoot, `property` holds the offending
// x:Class VALUE; what() renders "x:Class on a non-root element '<class>'".
class VAML_CORE_EXPORT VamlReaderException : public std::runtime_error
{
public:
    VamlReaderException(VamlReaderError kind, std::string property, std::string what);

    VamlReaderError kind() const noexcept { return kind_; }
    // The offending attribute's key (unknown-property), the duplicate x:Name
    // value (DuplicateName), or the offending x:Class value (ClassOnNonRoot).
    const std::string& property() const noexcept { return property_; }

private:
    VamlReaderError kind_;
    std::string property_;
};

// S092 (Strand B) — ONE retained deferred markup-extension on a built object.
// `target` is the object the extension was authored on, `key` the attribute
// key (e.g. "Text"), and `source` the ORIGINAL extension source text (e.g.
// "{Binding Title}") retained verbatim — the "S2 tag" so S252/S253 (the
// Binding sprint) can re-parse and bind it. The reader does NOT resolve the
// extension (that is S252's contract); it only makes it findable. Resolved by
// S252 — recorded NO-GAP, not silent.
struct DeferredExtension
{
    const DependencyObject* target = nullptr;
    std::string_view key;    // S140: interned (pool-backed, no owned churn)
    std::string_view source; // S140: interned
};

// S092 (Strand C) — ONE deferred event handler on a built object. `target` is
// the object the event was authored on, `eventName` the registered event name
// (e.g. "Click"), and `handlerName` the authored handler name (e.g. "OnClick").
// S322 resolves each record against the x:Class owner registry to wire the real
// routed event handler (S320 RoutedEvent primitive). RESOLVED at S322: this was
// the S092 MINIMAL REAL event-hook seed, KEPT + EXTENDED into the real surface
// that feeds BindEvents(). Do NOT backfill; do NOT revert to a stub.
struct DeferredEventHandler
{
    const DependencyObject* target = nullptr;
    std::string_view eventName;   // S140: interned
    std::string_view handlerName; // S140: interned
};

// --------------------------------------------------------------------------
// 11d-DEBUG — READER SCAN / AGGREGATE-ERRORS MODE (viewer "show all errors",
// `--fail_on_first_error` default off). Option A scope: A SINGLE DISCOVERED
// DOCUMENT reports ALL of its unknown-property (and sibling authoring) errors in
// ONE pass instead of throwing at the first. The reader KEEPS its strict
// fail-fast default (vamlcc / compiled / release / every existing consumer are
// bit-identical); this lenient mode is OPT-IN via set_collect_issues(true).
//
// In collect mode build() RECORDS each offender into issues() and CONTINUES
// scanning the rest of the element's attributes + descends into children, so a
// whole document's errors surface at once (the "find all errors" goal). The
// viewer's load_text/load_file set collect mode and, on error, return the full
// aggregated list with ok=false (a strict-fail result, never a silent success).
//
// CONTRACT BOUNDARY (recorded, NOT silent): collect mode consumes the
// attribute-level + property-element unknown-property genre in build() — the
// exact class that produced `unknown property 'AllowsTransparency'` — which is
// the reader's OWN error (VamlReaderError::UnknownProperty) and is safe to
// continue past. The DEEPER authored-resource genres (Style Setter/Trigger,
// Brush, ResourceDictionary capture, GradientStop) and the lower-layer failures
// (unknown ELEMENT -> ElementResolutionException, malformed VALUE ->
// ValueConversionException) stay fail-fast in BOTH modes: they are structural,
// not safe to skip mid-build without risking a partial tree. Those are
// surfaced (ever non-silent) on their first occurrence exactly as before. This
// is a deliberate, bounded scope — recorded here, not a silent backfill.
struct ScanIssue
{
    VamlReaderError kind = VamlReaderError::None;
    std::string what;   // the same human message the strict throw would carry (never empty)
};

class VAML_CORE_EXPORT VamlReader
{
public:
    VamlReader();
    ~VamlReader();

    VamlReader(const VamlReader&) = delete;
    VamlReader& operator=(const VamlReader&) = delete;

    // Parses `text` and builds a live tree — equivalent to parse_vaml(text)
    // then Load(root). Parse failures throw XmlScanException (S081) /
    // XmlParseException (S085); resolution/conversion failures propagate their
    // structured errors (see the header comment).
    [[nodiscard]] std::unique_ptr<DependencyObject> Load(std::string_view text);

    // Builds a live tree from an already-parsed root. The headline S091
    // surface. See the header comment for the algorithm + ownership model.
    // S092: also records the x:Name registry (Strand A), deferred markup
    // extensions (Strand B) and deferred event handlers (Strand C).
    [[nodiscard]] std::unique_ptr<DependencyObject> Load(const ParsedElement& root);

    // ------------------------------------------------------------------
    // 11d-DEBUG — reader SCAN / AGGREGATE-ERRORS mode (viewer "show all
    // errors", `--fail_on_first_error` default off). Option A scope, see the
    // ScanIssue comment. Strict FAIL-FAST remains the DEFAULT for every
    // existing consumer; collect mode is OPT-IN.
    // ------------------------------------------------------------------

    // Enables (true) or disables (false, default) LENIENT scan mode. In collect
    // mode build() records each reader-owned unknown-property offender into
    // issues() and keeps scanning the document (one pass, ALL errors), instead
    // of throwing at the first. NOT per-Load cleared here (an app sets it once
    // for the scan session); issues() IS cleared at each Load's start. The
    // structural/lower-layer genres stay fail-fast in both modes (see the
    // ScanIssue comment — a recorded, deliberate boundary, not a silent gap).
    void set_collect_issues(bool on) noexcept { collectIssues_ = on; }

    // True iff collect mode is enabled.
    [[nodiscard]] bool collect_issues() const noexcept { return collectIssues_; }

    // The reader-owned issues recorded by the most recent Load in collect mode.
    // Cleared at the start of each Load. Empty in strict mode (and empty when a
    // collect-mode Load found nothing). Document order (first offender first).
    [[nodiscard]] const std::vector<ScanIssue>& issues() const noexcept { return issues_; }

    // The non-root built objects, in construction (document) order. Each is
    // owned exactly once by this reader during parse (the S091/D1 model); the
    // returned root is separate. S440 adds ONE explicit, honest hand-off:
    // `finalize_visual_tree()` — see below — transfers the owned children into
    // their parents' S121 visual-children lists so the tree RENDERS its
    // declared children (not just the root). Before finalize, retained_ keeps
    // owning (all pre-S440 commit/parse tests are unchanged). After finalize,
    // the visual tree owns them and retained_ is emptied (non-owning).
    [[nodiscard]] const std::vector<std::unique_ptr<DependencyObject>>&
    retained_objects() const noexcept { return retained_; }

    // 11j-7 — the ELEMENT-LEVEL TRIGGERS authored via `<Owner.Triggers>`
    // property-elements (G7, dep:S454), in document order: (element, its owned
    // trigger set). The triggers are the SAME live S362 model a Style carries
    // (PropertyTrigger/DataTrigger + child setters), parsed as REAL authoring;
    // the element-level EVALUATION (re-evaluate on DP match) is the recorded
    // continuation to the control-material/content step. Per-Load scope.
    [[nodiscard]] const std::vector<std::pair<DependencyObject*, std::vector<Trigger>>>&
    element_triggers() const noexcept { return elementTriggers_; }

    // ------------------------------------------------------------------
    // S092 Strand A — x:Name registry (REAL, tested, permanent).
    // ------------------------------------------------------------------

    // Returns the built object registered under `name`, or nullptr if no such
    // x:Name was recorded. Works for the root (a root carrying x:Name is
    // registered) and every retained child. NAMES WERE CLEARED at the start of
    // the most recent Load — this reflects the CURRENT read.
    [[nodiscard]] DependencyObject* named(std::string_view name) const noexcept;

    // Number of currently-registered names (0 after a Load with no x:Name).
    [[nodiscard]] std::size_t name_count() const noexcept;

    // Registered x:Name entries in REGISTRATION (document) order: (name, obj).
    // Keys are interned string_views (S144 — the S140 discipline: pool-backed,
    // stable for the interning thread, content-equal; NO owned std::string per
    // entry, no owned-name gotcha).
    [[nodiscard]] const std::vector<std::pair<std::string_view, DependencyObject*>>&
    names() const noexcept;

    // The authored x:Class name on the root, empty when absent. S092 records
    // ONLY the authored name; binding it to an owner object/type is deferred
    // (Sprint 8 / S286 / InitializeComponent-analogue). Recorded NO-GAP.
    [[nodiscard]] const std::string& root_class() const noexcept;

    // ------------------------------------------------------------------
    // S440 — the S120/S121 visual-tree hand-off (closes the roadmap-recorded
    // NO-GAP: "children transfer into the UIElement visual tree at S120/S121").
    // REAL permanent core surface — NOT a MINIMAL REAL seed: this is the funded
    // S11 deliverable that makes a VAML-loaded tree RENDER its declared
    // children (not just the root).
    // ------------------------------------------------------------------

    // TRANSFERS every non-root child that is BOTH a UIElement AND owns a
    // UIElement parent into that parent's S121 visual-children list via
    // add_child (parent-link + S041 inheritance-scope edge + tree ownership),
    // so the loaded tree measures/arranges/renders/hit-tests its children.
    // After the transfer each transferred child is REMOVED from the reader's
    // owned-retained set (the visual tree now owns it) — the reader keeps no
    // dangling half-owner. Idempotent (a second call after a successful
    // first is a no-op). Bind/home AFTER the tree is fully built (after Load),
    // BEFORE rendering/reload writes children.
    //
    // During construction `retained_` owns every child (D1); callers that only
    // PARSE (mount structural/introspection tests) never call this and keep the
    // owned set. Callers that WANT a renderable tree (the app-shell, reload,
    // the compiled reader, the renderers) call it and retain_ is emptied of
    // transferred children. Returns the number of children transferred.
    //
    // Ownership is NOT silent: a child that stays in `retained_` (a
    // non-UIElement, or a UIElement whose parent is non-UIElement) remains
    // reader-owned and is NOT in the visual tree — the call reports it via the
    // S250_1 diagnostics channel if an app diagnostic function is installed.
    [[nodiscard]] std::size_t finalize_visual_tree();

    // ------------------------------------------------------------------
    // S092 Strand B — deferred markup extensions (the S2 tag). Resolved by
    // S252. Retained only; never converted/set here.
    // ------------------------------------------------------------------
    [[nodiscard]] const std::vector<DeferredExtension>& deferred_extensions() const noexcept;

    // ------------------------------------------------------------------
    // S252 — markup {Binding} resolution (the S2 tag now bound). REAL +
    // permanent core surface (NOT MINIMAL REAL — resolves the S092-recorded
    // "S2 tag: resolved by S252" hand-off at the feature's own step).
    // ------------------------------------------------------------------

    // Supplies the app-constructed ViewModel (an ObservableObject — the S250
    // app-owned composition pattern) that markup {Binding <path>} resolves
    // against. NO DI / NO container: the app owns the VM and sets it here
    // before BindExtensions(). Persists across calls until changed/cleared.
    // Binding to a non-ObservableObject data context is a Class B NotReadable
    // runtime outcome (never a silent no-op).
    void set_data_context(ObservableObject* vm) noexcept { dataContext_ = vm; }

    // The current data context (nullptr when the app has not set one).
    [[nodiscard]] ObservableObject* data_context() const noexcept { return dataContext_; }

    // RESOLVES every recorded deferred {Binding <path>[, Mode=...]} extension
    // into a LIVE BindingExpression (S242/S243/S244) against the current data
    // context + the target object/property the extension was authored on. The
    // bindings are reader-owned (RAII) and cleared at the start of each Load
    // (per-Load live scope, same as the other S092 registries). Returns the
    // number judged to be THIS layer's extension rows: bound {Binding} rows.
    //
    // Outcomes (see the BindingMarkup grammar):
    //   - kind Binding + well-formed -> bind + count as bound.
    //   - kind Binding + MALFORMED    -> throws VamlReaderException(
    //     MalformedBindingMarkup) naming source + target key (Class A authoring
    //     error, NEVER silently left deferred).
    //   - kind Command                -> left deferred (S253); NOT counted, NOT
    //     an error.
    //   - kind None (StaticResource, ...) -> left deferred (S361); NOT counted.
    //
    // WPF-faithful default mode is OneWay; `Mode=` overrides it. Diagnostics
    // route through the S250_1 channel (vaml::view_binding_diagnostics /
    // vaml::set_view_binding_diagnostics — app opt-in, never a DI slot). The
    // policy comes from vaml::view_binding_policy_default() (Fallback).
    [[nodiscard]] std::size_t BindExtensions();

    // The reader-owned live bindings created by BindExtensions() — includes a
    // stable "object identity" (the live VM) + source path + target key so the
    // S253/S256 consumers (and tests) can introspect what was bound.
    [[nodiscard]] const std::vector<std::unique_ptr<BindingExpression>>&
    live_bindings() const noexcept { return live_bindings_; }

    // ------------------------------------------------------------------
    // S461 (dep:S460) — MARKUP EXTENSIONS {x:Static} + {x:Type} (the S2-tag
    // deferral CLOSED — the remaining attribute surface). The static-field/const
    // access + the type object both resolve through the READER (app-registered,
    // S250: no reflection, no DI). REAL permanent core surface.
    // ------------------------------------------------------------------

    // Registers a static value the markup `{x:Static <token>}` + the reader can
    // resolve: `token` is the LAST `:`-separated segment after any `local:` /
    // namespace prefix (WPF-faithful — `{x:Static local:Constants.AppName}`
    // resolves the token `Constants.AppName`). The app (the S250 pattern) calls
    // this BEFORE ResolveStatic. Idempotent (first registration wins); the value
    // is copied by-value. NOT per-Load cleared (app-owned config, like register_command).
    void register_static_value(std::string token, Value value);

    // Resolves a registered static VALUE by its token (the last `:`-segmented
    // token, e.g. "Constants.AppName"). Returns Value::Null() when unregistered.
    [[nodiscard]] Value ResolveStatic(std::string_view token) const noexcept;

    // Registers a type-object the markup `{x:Type <name>}` + the reader can
    // resolve: `name` -> a `Value::String(<name>)` type object used for
    // TargetType / ContentControl binding (the S461 {x:Type Button}). The app
    // registers the known authorable type names (via the factory registry's
    // registered_names implicitly, plus any explicit ones). Idempotent.
    void register_type_value(std::string name, std::string typeName);

    // Resolves a registered {x:Type <name>} to its type object
    // (Value::String(typeName)), or Value::Null() when unregistered.
    [[nodiscard]] Value ResolveType(std::string_view name) const noexcept;

    // RESOLVES every recorded deferred {x:Static token} / {x:Type name}
    // extension — a S252 kind-Static/Type row now owned here — by pushing the
    // resolved value onto the authored target's DP (the extension's key). An
    // UNRESOLVED static/type is a Class A authoring error -> a structured
    // VamlReaderException (MalformedBindingMarkup-style), never silent. Returns
    // the number of static/type rows resolved this call. A {Binding}/{Command}/
    // {StaticResource} row is NOT this layer's — it stays deferred.
    [[nodiscard]] std::size_t ResolveStaticTypeExtensions();

    // ------------------------------------------------------------------
    // S361 — markup {StaticResource}/{DynamicResource} resolution (the S252
    // kind-None deferral now resolved). REAL permanent core surface.
    // ------------------------------------------------------------------

    // RESOLVES every recorded deferred {StaticResource <key>} / {DynamicResource
    // <key>} extension (a S252 kind-None row this layer now owns) against the
    // target object + the S360 three-scope lookup. STATIC resolves once at
    // apply time; a MISSING static key throws ResourceLookupException(MissingKey)
    // (the SHARED decision-3 error, Class A). DYNAMIC applies now + records a
    // DynamicResourceLink; reapply_dynamic_resources() re-pushes on a source
    // change; a transiently-missing dynamic key HOLDS (never throws).
    //
    // Returns the number of resource rows applied this call. A {Binding} /
    // {Command} row is NOT this layer's — it stays deferred (S252/S253 own it).
    // The recorded dynamic links are reader-owned (per-Load, cleared at Load
    // start). The app/viewer calls reapply_dynamic_resources after a source
    // dictionary key change.
    [[nodiscard]] std::size_t BindResources();

    // RE-EVALUATES every recorded dynamic resource link (S361) against the
    // current three-scope lookup and re-pushes the resolved value. Never throws
    // on a transiently-missing key (holds). The app/viewer calls this after a
    // source dictionary key change (the S361 re-evaluation seam). Returns the
    // number of links re-applied.
    [[nodiscard]] std::size_t ReapplyDynamicResources();

    // ------------------------------------------------------------------
    // S253 — {Command} markup resolution (the S252 command surface). REAL +
    // permanent core surface (NOT MINIMAL REAL — the S2-tag {Command} arm).
    // ------------------------------------------------------------------

    // Resolves ONE {Command <name>} markup extension (left deferred by
    // S252's BindExtensions) against the data context: it looks the command
    // up BY NAME among the VM's owned ICommand entries. Returns a non-owning
    // pointer to the command, or nullptr if no such command is registered.
    // Used by the app/test to invoke a markup-declared command; the live
    // ICommand interface (S246) is the S253 command surface.
    [[nodiscard]] ICommand* ResolveCommand(std::string_view name);

    // Registers an app-owned command (the S250 app-owned composition pattern)
    // that markup {Command <name>} can resolve. The app author constructs the
    // RelayCommand + registers it here; the reader stores a NON-OWNING pointer
    // (the app owns the command's lifetime). Idempotent for a repeated name
    // (the first registration wins). Cleared at the start of each Load.
    void register_command(std::string_view name, ICommand* command) noexcept;

    // ------------------------------------------------------------------
    // S464 (dep:S463) — COMMAND + CommandParameter attributes.
    // ------------------------------------------------------------------
    // The `Command="{Binding SaveCommand}"` + `CommandParameter="{Binding
    // SelectedItem}"` markup: the Command's Binding path is the command NAME
    // (the app-registered S253 command registry), and CommandParameter provides
    // a parameter VALUE (a resolved binding, a static value, or a literal). The
    // S246 ICommand::Execute(Value) surface is invoked with the parameter — the
    // "parameterized execute" extension of S253's {Command}.

    // Executes the registered command under `name` with `parameter`. Returns
    // true when the command resolved + executed (CanExecute permitting). When
    // `parameter` is Null, a Null parameter is passed (the command's default).
    // A command that CanExecute()==false with the parameter does NOT execute
    // (WPF-faithful) but STILL resolves (returns false). Returns false when no
    // command is registered under `name` (never throws).
    [[nodiscard]] bool ExecuteCommand(std::string_view name, const Value& parameter);

    // ------------------------------------------------------------------
    // S092 Strand C + S322 — event hooks. RESOLVED by S322 (Sprint 8): the
    // S092 MINIMAL REAL event-hook seed (register_event + deferred-event
    // records) is now the REAL surface feeding the S322 class-owner registry
    // + BindEvents. The sentinel is REMOVED at this step (NO-GAP); the seed is
    // kept + extended, never backfilled.
    // ------------------------------------------------------------------

    // Declares that `eventName` is an EVENT, not a property. An attribute whose
    // key exactly matches a registered event name is routed to a deferred-event
    // record in build() (never a property set / unknown-property throw). The
    // host (the app / future control type) calls this before Load. Idempotent.
    void register_event(std::string_view eventName);

    // The deferred event handlers recorded by the most recent Load, document
    // order. Consumed by BindEvents() (S322).
    [[nodiscard]] const std::vector<DeferredEventHandler>&
    deferred_event_handlers() const noexcept;

    // ------------------------------------------------------------------
    // S322 — markup event wiring: Click="OnClick" -> handler on x:Class owner
    // (real routed-event binding on the S320/S321 engine).
    // ------------------------------------------------------------------

    // Registers a named class-owner HANDLER for a markup event. The x:Class
    // decomposes as: the sheet's root x:Class name is "this" (root_class_);
    // every handler is registered under (handlerName) for that class. The app
    // (ADR-006 app-owned composition, NO DI) calls this BEFORE BindEvents()
    // so markup `Click="OnClick"` resolves to a real RoutedEventHandler.
    // Idempotent (first-wins); a repeat with the same handlerName is a no-op.
    void bind_class_handler(std::string_view handlerName, RoutedEventHandler handler);

    // RESOLVES every deferred_event_handlers() record into a LIVE routed
    // handler on the loaded tree, via the S320 RoutedEvent registry + the
    // S321 UIElement routed-handler surface. Called AFTER BindExtensions()
    // (the reader has the deferred records from the most recent Load).
    //
    //   - eventName -> a registered RoutedEvent (ClickEvent/KeyDownEvent/...);
    //     an UNKNOWN event name is NOT bound and reported via the S250_1
    //     binding-diagnostics channel (never silent — the event must be
    //     registered; a structured NoEventHandler-class note names it).
    //   - handlerName -> a (class, handlerName) registered RoutedEventHandler;
    //     an UNREGISTERED handlerName throws VamlReaderException(
    //     NoEventHandler) naming the class + handler (Class A authoring, fail
    //     loud, never silent).
    //   - attaches target->add_routed_handler(event, handler) on the element
    //     the markup was authored on (the S092 record's target).
    //
    // The wired handlers are per-Load (cleared at the next Load's start);
    // the class-owner handler REGISTRATIONS survive (app-owned config, like
    // register_command). Returns the number of wired records.
    [[nodiscard]] std::size_t BindEvents();

private:
    // Recursive build (the algorithm in the header comment). Returns the owned
    // live object; appends non-root children to `retained_`. Throws the
    // structured errors described above. S094: `depth` is 0 for the root,
    // incremented per recursion level; it gates the ClassOnNonRoot rule
    // (x:Class is root-only — a Class directive at depth != 0 throws).
    std::unique_ptr<DependencyObject> build(const ParsedElement& element, std::size_t depth);

    // Converts a plain string literal into a Value of the DP's bound type.
    // String -> identity; otherwise routes through ValueConverterRegistry::global()
    // via the type-name mapping. Throws ValueConversionException (S089/S090).
    static Value convertLiteral(const DependencyProperty& dp, std::string_view literal);

    // S453 (dep:S452) — RESOURCE-DICTIONARY AUTHORING (the S366 NO-GAP
    // resolved): builds an OWNED ResourceDictionary from a parsed
    // `<ResourceDictionary>` element (a non-DependencyObject container, so it
    // gets a dedicated build path). `resourceDict` is the parsed element whose
    // children are authored resources (each carrying x:Key) and/or nested
    // `<ResourceDictionary.MergedDictionaries>` property-elements. `parent` is
    // the object being built (used to parent-link any owned resource objects;
    // typically the element whose `.Resources` this dictionary becomes, or
    // nullptr for a standalone dictionary). Returns the owned dictionary.
    //
    // Each authored child is captured into the dictionary by its x:Key (the
    // S361 hand-off RESOLVED): a child carrying `x:Key="..."` is built (via
    // build()) and `set(key, owned object)` (an Object resource). A duplicate
    // key -> ResourceAlreadyRegistered (the S360 dedupe-throws-unchanged
    // contract). A child with NO x:Key in a dictionary is an authoring error ->
    // a structured VamlReaderException (UnknownProperty-style, naming the key).
    std::unique_ptr<ResourceDictionary> build_resource_dictionary(
        const ParsedElement& resourceDict, DependencyObject* parent, std::size_t depth);

    // S454 — resolves every deferred Style InlineDefinitionBasedOn request
    // (collected during build_style) once all authored Style resources exist:
    // wires each derived Style's based_on pointer to the base Style found by
    // key across the built dictionaries. Called at the end of Load.
    void resolve_deferred_based_on();

    // 11d-DEBUG — attribute-level unknown-property report/throw decision.
    // In strict mode (default) throws VamlReaderException(UnknownProperty, key,
    // what) exactly as before. In collect mode records {What, what} into
    // issues_ and RETURNS (the caller continues scanning). The two structural
    // genres it is NOT used for (resource-authoring + lower-layer) stay
    // fail-fast; only the reader-OWNED attribute/property-element unknown-
    // property genre is collectible (see ScanIssue's contract note).
    void report_unknown_property(std::string key, std::string what);

    // S454 (dep:S453) — STYLE AUTHORING + TRIGGERS (the S362/S363 engine AUTHORED
    // from .vaml). Consumes a parsed `<Style>` element (already resolved to a
    // Style object) into the live S362 data model:
    //   * `TargetType="X"` -> style.set_target_type();
    //   * each `<Setter Property="P" Value="V"/>` child -> style.add_setter(
    //     { PropertyId(P), SetterValue } ) where a `{StaticResource K}` /
    //     `{DynamicResource K}` / `{TemplateBinding T}` Value becomes the
    //     respective reference SetterValue and any other literal converts to the
    //     property's bound type;
    //   * `<Style.Triggers>` property-element with `<PropertyTrigger Property
    //     Value>` (+ its child <Setter>s) and `<DataTrigger Binding=Path Value>`
    //     -> style.add_trigger (S362 Discrimination).
    // `BasedOn="{StaticResource base}"` is recorded into `deferredBasedOn_` and
    // wired (style.set_based_on) by resolveDeferredBasedOn() at the end of Load,
    // once every authored Style (a dict resource) is present.
    void build_style(Style& style, const ParsedElement& element);

    // S454 — parses ONE `<Setter Property="P" Value="V"/>` element into a live
    // Setter (used for both top-level Style setters and trigger child setters).
    void parse_setter(const ParsedElement& el, std::string owner, Setter& out);

    // S460 (dep:S456) — BRUSH AUTHORING. Consumes ONE authored `<GradientStop/>`
    // element (already resolved to a GradientStopHost) into its Color/Offset DPs.
    // Attributes: `Color="#RRGGBB"` + `Offset="0.5"`. Never a visual child.
    void build_gradient_stop(GradientStopHost& stop, const ParsedElement& element);

    // S460 (dep:S456) — BRUSH AUTHORING. Consumes ONE authored
    // `<LinearGradientBrush/>` element (already resolved to a
    // LinearGradientBrush) into its data model: `StartPoint="x,y"` /
    // `EndPoint="x,y"` attributes + a `<LinearGradientBrush.GradientStops>`
    // property-element whose `<GradientStop>` children are folded into the
    // brush's ordered stop vector. Never a visual child.
    void build_linear_gradient(LinearGradientBrush& brush, const ParsedElement& element);

    // 11k-4 (dep:11k-3/S453/S460) — VECTOR-DRAWING AUTHORING. These consume an
    // authored vector/drawing/brush RESOURCE element into its owned data model
    // (the S460 build_linear_gradient precedent). Each is a RESOURCE (captured
    // by x:Key), never a visual. They wire the authored child property-elements
    // (<DrawingBrush.Drawing> / <GeometryDrawing.Geometry> / <DrawingGroup's
    // children>) into the owned shared_ptr child graph.
    //
    // `Geometry` (abstract, WPF-faithful) is NOT authored directly; the concrete
    // DO leaves (PathGeometry/GeometryGroup/RectangleGeometry/EllipseGeometry) are.
    void build_path_geometry(PathGeometry& geom, const ParsedElement& element);
    void build_geometry_group(GeometryGroup& group, const ParsedElement& element, std::size_t depth);
    void build_rectangle_geometry(RectangleGeometry& geom, const ParsedElement& element);
    void build_ellipse_geometry(EllipseGeometry& geom, const ParsedElement& element);
    void build_geometry_drawing(GeometryDrawing& drawing, const ParsedElement& element, std::size_t depth);
    void build_drawing_group(DrawingGroup& group, const ParsedElement& element, std::size_t depth);
    void build_drawing_brush(DrawingBrush& brush, const ParsedElement& element, std::size_t depth);
    void build_drawing_image(DrawingImage& image, const ParsedElement& element, std::size_t depth);
    void build_image_brush(ImageBrush& brush, const ParsedElement& element);

    // 11k-4 — builds an authored child element (recursively via build()),
    // RETAINS it READER-OWNED, parent-links it, and returns the raw pointer.
    // Used by the drawing/geometry builders to wire owned child graphs.
    DependencyObject* build_retained_child(const ParsedElement& el,
                                           DependencyObject* parent,
                                           std::size_t depth);
    // 11k-4 — builds + retains every child of a property-element (an authored
    // `<Owner.Geometry>`/`<Owner.Drawing>` object slot) and returns the raw
    // pointers in order.
    std::vector<DependencyObject*> build_property_children(const ParsedElement& el,
                                                           DependencyObject* parent,
                                                           std::size_t depth);

    // The non-root built objects, in construction (document) order. OWNED by
    // the reader (D1/S091) until `finalize_visual_tree()` hands each child into
    // its parent's S121 visual-children list; after finalize this vector is
    // emptied (the visual tree owns the children).
    std::vector<std::unique_ptr<DependencyObject>> retained_;

    // S440: set true once finalize_visual_tree() has transferred the built set;
    // a second call is an idempotent no-op. Reset at the start of each Load.
    bool visual_finalized_ = false;

    // S453 — reader-owned merged ResourceDictionaries (per-Load). A
    // `<ResourceDictionary.MergedDictionaries>` child dictionary is BORROWED by
    // its parent via the S360 merge_with (non-owning), so this vector keeps each
    // merged dictionary ALIVE for the parent's lifetime (cleared at Load start,
    // like the other per-Load scopes).
    std::vector<std::unique_ptr<ResourceDictionary>> owned_merged_;

    // S454 — reader-owned authored dictionaries (per-Load) used to resolve a
    // Style BasedOn by key: every ResourceDictionary this Load BUILT
    // (owned + merged) is listed so a derived Style can find its base Style* by
    // key. Cleared at Load start. (The dicts themselves live in owned_merged_ +
    // the S360 set_element_resources side-table; this is a search index, not a
    // second ownership.)
    std::vector<ResourceDictionary*> built_dicts_;

    // S454 — deferred Style BasedOn wires: (derived Style*, base key) recorded
    // during build_style; resolved by resolve_deferred_based_on() at end of Load
    // once all authored Style resources exist.
    std::vector<std::pair<Style*, std::string>> deferred_based_on_;

    // S092 Strand A — x:Name registry (insertion-ordered; interned string_view
    // keys per S144/S140 — pool-backed, content-equal, stable for this reader's
    // thread).
    std::vector<std::pair<std::string_view, DependencyObject*>> names_;
    std::string root_class_;

    // S092 Strand B — deferred markup extensions (document order).
    std::vector<DeferredExtension> deferred_extensions_;

    // 11d-DEBUG — collect mode flag (lenient scan, opt-in; strict default) +
    // the accumulated ScanIssues (cleared at each Load's start).
    bool collectIssues_ = false;
    std::vector<ScanIssue> issues_;

    // S092 Strand C — registered event names + deferred event handlers.
    std::vector<std::string> event_names_;
    std::vector<DeferredEventHandler> deferred_event_handlers_;

    // S322 — the class-owner handler registry: (handlerName, RoutedEventHandler)
    // for the root class. App-owned config (ADR-006, NO DI); non-owning
    // std::function copy (the app's handler is copy-stored); survives Loads.
    std::vector<std::pair<std::string, RoutedEventHandler>> class_handlers_;

    // S252 — the data context (the app-constructed VM, non-owning) + the
    // reader-owned live bindings Created by BindExtensions(). Cleared at the
    // start of each Load (per-Load live scope).
    ObservableObject* dataContext_ = nullptr;
    std::vector<std::unique_ptr<BindingExpression>> live_bindings_;

    // S253 — the {Command <name>} registry: (name, non-owning ICommand*).
    // App-owned, configured before Load (like register_event), NOT per-Load
    // cleared — a command registration survives the tree it is authored on.
    std::vector<std::pair<std::string, ICommand*>> commands_;

    // S461 — the {x:Static <token>} + {x:Type <name>} registries: (token, Value)
    // + (name, typeName). App-owned (S250), configured before Load; NOT per-Load
    // cleared (a static/type registration survives the tree it is authored on,
    // like register_command).
    std::vector<std::pair<std::string, Value>> static_values_;
    std::vector<std::pair<std::string, std::string>> type_values_;

    // S361 — the reader-owned dynamic-resource links (created by
    // BindResources), re-evaluated by ReapplyDynamicResources. Per-Load
    // (cleared at Load start).
    std::vector<DynamicResourceLink> dynamicResourceLinks_;

    // 11j-7 — ELEMENT-LEVEL TRIGGERS (G7, dep:S454). A per-Load store of the
    // authored `<Owner.Triggers>` property-element triggers, keyed by the
    // element they were authored on. The triggers are parsed into the SAME live
    // S362 trigger model (style.add_trigger), so the authoring surface is fully
    // real + parseable; full element-level trigger EVALUATION on a live element
    // is a recorded continuation to the control-material/content step (S454
    // styles already evaluate; element-level evaluation lands there). Per-Load
    // (cleared at Load start like the other registries).
    std::vector<std::pair<DependencyObject*, std::vector<Trigger>>> elementTriggers_;
};

} // namespace vaml
