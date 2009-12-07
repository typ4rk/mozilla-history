/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is HTML Parser Gecko integration code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsHtml5TreeOpExecutor_h__
#define nsHtml5TreeOpExecutor_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsTraceRefcnt.h"
#include "nsHtml5TreeOperation.h"
#include "nsHtml5PendingNotification.h"
#include "nsTArray.h"
#include "nsContentSink.h"
#include "nsNodeInfoManager.h"
#include "nsHtml5DocumentMode.h"
#include "nsITimer.h"
#include "nsIScriptElement.h"
#include "nsIParser.h"
#include "nsCOMArray.h"
#include "nsAHtml5TreeOpSink.h"
#include "nsHtml5TreeOpStage.h"

class nsHtml5TreeBuilder;
class nsHtml5Tokenizer;
class nsHtml5StreamParser;

typedef nsIContent* nsIContentPtr;

class nsHtml5TreeOpExecutor : public nsContentSink,
                              public nsIContentSink,
                              public nsAHtml5TreeOpSink
{
  public:
    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)

  private:
#ifdef DEBUG_hsivonen
    static PRUint32    sInsertionBatchMaxLength;
    static PRUint32    sAppendBatchMaxSize;
    static PRUint32    sAppendBatchSlotsExamined;
    static PRUint32    sAppendBatchExaminations;
#endif
    static PRUint32                      sTreeOpQueueMaxLength;

    /**
     * Whether EOF needs to be suppressed
     */
    PRBool                               mSuppressEOF;
    
    PRBool                               mHasProcessedBase;
    PRBool                               mReadingFromStage;
    nsCOMPtr<nsITimer>                   mFlushTimer;
    nsTArray<nsHtml5TreeOperation>       mOpQueue;
    nsTArray<nsIContentPtr>              mElementsSeenInThisAppendBatch;
    nsTArray<nsHtml5PendingNotification> mPendingNotifications;
    nsHtml5StreamParser*                 mStreamParser;
    nsCOMArray<nsIContent>               mOwnedElements;
    
    // This could be optimized away by introducing more tree ops so that 
    // non-elements wouldn't use the handle setup but the text node / comment
    // / doctype operand would be remembered by the tree op executor.
    nsCOMArray<nsIContent>               mOwnedNonElements;
  
    /**
     * Whether the parser has started
     */
    PRBool                        mStarted;

    /**
     * Script to run ASAP
     */
    nsCOMPtr<nsIContent>          mScriptElement;
    
    nsHtml5TreeOpStage            mStage;

    PRBool                        mFlushing;
    
    PRBool                        mInDocumentUpdate;

    /**
     * Used for deferring DidBuildModel call out of notification batch
     */
    PRBool                        mCallDidBuildModel;
    
    nsCString                     mCharsetSwitch;

  public:
  
    nsHtml5TreeOpExecutor();
    virtual ~nsHtml5TreeOpExecutor();
  
    // nsIContentSink

    /**
     * Unimplemented. For interface compat only.
     */
    NS_IMETHOD WillParse();

    /**
     * 
     */
    NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) {
      NS_ASSERTION(GetDocument()->GetScriptGlobalObject(), 
                   "Script global object not ready");
      mDocument->AddObserver(this);
      WillBuildModelImpl();
      GetDocument()->BeginLoad();
      return NS_OK;
    }

    /**
     * Emits EOF.
     */
    NS_IMETHOD DidBuildModel(PRBool aTerminated);

    /**
     * Forwards to nsContentSink
     */
    NS_IMETHOD WillInterrupt();

    /**
     * Unimplemented. For interface compat only.
     */
    NS_IMETHOD WillResume();

    /**
     * Sets the parser.
     */
    NS_IMETHOD SetParser(nsIParser* aParser);

    /**
     * No-op for backwards compat.
     */
    virtual void FlushPendingNotifications(mozFlushType aType);

    /**
     * Sets mCharset
     */
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset);

    /**
     * Returns the document.
     */
    virtual nsISupports *GetTarget();
  
    // nsContentSink methods
    virtual nsresult ProcessBASETag(nsIContent* aContent);
    virtual void UpdateChildCounts();
    virtual nsresult FlushTags();
    virtual void PostEvaluateScript(nsIScriptElement *aElement);
    /**
     * Sets up style sheet load / parse
     */
    void UpdateStyleSheet(nsIContent* aElement);

    // Getters and setters for fields from nsContentSink
    nsIDocument* GetDocument() {
      return mDocument;
    }
    nsNodeInfoManager* GetNodeInfoManager() {
      return mNodeInfoManager;
    }
    nsIDocShell* GetDocShell() {
      return mDocShell;
    }

    PRBool IsScriptExecuting() {
      return IsScriptExecutingImpl();
    }
    
    void SetBaseUriFromDocument() {
      mDocumentBaseURI = mDocument->GetBaseURI();
      mHasProcessedBase = PR_TRUE;
    }
    
    void SetNodeInfoManager(nsNodeInfoManager* aManager) {
      mNodeInfoManager = aManager;
    }
    
    void SetStreamParser(nsHtml5StreamParser* aStreamParser) {
      mStreamParser = aStreamParser;
    }
    
    inline void SetScriptElement(nsIContent* aScript) {
      mScriptElement = aScript;
    }
    
    void InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState, PRInt32 aLine);

    PRBool IsScriptEnabled();

    inline void BeginDocUpdate() {
      NS_PRECONDITION(!mInDocumentUpdate, "Tried to double-open update.");
      NS_PRECONDITION(mParser, "Started update without parser.");
      mInDocumentUpdate = PR_TRUE;
      mDocument->BeginUpdate(UPDATE_CONTENT_MODEL);
    }

    inline void EndDocUpdate() {
      if (mInDocumentUpdate) {
        mInDocumentUpdate = PR_FALSE;
        mDocument->EndUpdate(UPDATE_CONTENT_MODEL);
      }
    }

    void PostPendingAppendNotification(nsIContent* aParent, nsIContent* aChild) {
      PRBool newParent = PR_TRUE;
      const nsIContentPtr* first = mElementsSeenInThisAppendBatch.Elements();
      const nsIContentPtr* last = first + mElementsSeenInThisAppendBatch.Length() - 1;
      for (const nsIContentPtr* iter = last; iter >= first; --iter) {
#ifdef DEBUG_hsivonen
        sAppendBatchSlotsExamined++;
#endif
        if (*iter == aParent) {
          newParent = PR_FALSE;
          break;
        }
      }
      if (aChild->IsNodeOfType(nsINode::eELEMENT)) {
        mElementsSeenInThisAppendBatch.AppendElement(aChild);
      }
      mElementsSeenInThisAppendBatch.AppendElement(aParent);
      if (newParent) {
        mPendingNotifications.AppendElement(aParent);
      }
#ifdef DEBUG_hsivonen
      sAppendBatchExaminations++;
#endif
    }

    void FlushPendingAppendNotifications() {
      const nsHtml5PendingNotification* start = mPendingNotifications.Elements();
      const nsHtml5PendingNotification* end = start + mPendingNotifications.Length();
      for (nsHtml5PendingNotification* iter = (nsHtml5PendingNotification*)start; iter < end; ++iter) {
        iter->Fire();
      }
      mPendingNotifications.Clear();
#ifdef DEBUG_hsivonen
      if (mElementsSeenInThisAppendBatch.Length() > sAppendBatchMaxSize) {
        sAppendBatchMaxSize = mElementsSeenInThisAppendBatch.Length();
      }
#endif
      mElementsSeenInThisAppendBatch.Clear();
    }
    
    inline PRBool HaveNotified(nsIContent* aElement) {
      NS_PRECONDITION(aElement, "HaveNotified called with null argument.");
      const nsHtml5PendingNotification* start = mPendingNotifications.Elements();
      const nsHtml5PendingNotification* end = start + mPendingNotifications.Length();
      for (;;) {
        nsIContent* parent = aElement->GetParent();
        if (!parent) {
          return PR_TRUE;
        }
        for (nsHtml5PendingNotification* iter = (nsHtml5PendingNotification*)start; iter < end; ++iter) {
          if (iter->Contains(parent)) {
            return iter->HaveNotifiedIndex(parent->IndexOf(aElement));
          }
        }
        aElement = parent;
      }
    }

    void StartLayout() {
      nsIDocument* doc = GetDocument();
      if (doc) {
        FlushPendingAppendNotifications();
        nsContentSink::StartLayout(PR_FALSE);
      }
    }
    
    void DocumentMode(nsHtml5DocumentMode m);

    nsresult Init(nsIDocument* aDoc, nsIURI* aURI,
                  nsISupports* aContainer, nsIChannel* aChannel);
                  
    void Flush();

    void MaybeSuspend();

    void Start();

    void NeedsCharsetSwitchTo(const char* aEncoding);
    
    void PerformCharsetSwitch();
    
#ifdef DEBUG
    PRBool HasScriptElement() {
      return !!mScriptElement;
    }
#endif

    PRBool IsComplete() {
      return !mParser;
    }
    
    PRBool HasStarted() {
      return mStarted;
    }
    
    PRBool IsFlushing() {
      return mFlushing;
    }
    
    void RunScript();
    
    void MaybePreventExecution() {
      if (mScriptElement) {
        nsCOMPtr<nsIScriptElement> script = do_QueryInterface(mScriptElement);
        NS_ASSERTION(script, "mScriptElement didn't QI to nsIScriptElement!");
        script->PreventExecution();
        mScriptElement = nsnull;
      }    
    }
    
    void Reset();
    
    inline void HoldElement(nsIContent* aContent) {
      mOwnedElements.AppendObject(aContent);
    }

    inline void HoldNonElement(nsIContent* aContent) {
      mOwnedNonElements.AppendObject(aContent);
    }

    // The following two methods are for the main-thread case

    /**
     * No-op
     */    
    virtual void MaybeFlush(nsTArray<nsHtml5TreeOperation>& aOpQueue);

    /**
     * Flush the operations from the tree operations from the argument
     * queue unconditionally.
     */
    virtual void ForcedFlush(nsTArray<nsHtml5TreeOperation>& aOpQueue);
    
    nsAHtml5TreeOpSink* GetStage() {
      return &mStage;
    }
    
    void StartReadingFromStage() {
      mReadingFromStage = PR_TRUE;
    }

    void StreamEnded();
    
    void ScheduleTimer();

#ifdef DEBUG
    void AssertStageEmpty() {
      mStage.AssertEmpty();
    }
#endif

  private:

    nsHtml5Tokenizer* GetTokenizer();
        
};

#endif // nsHtml5TreeOpExecutor_h__